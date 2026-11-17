#include "gui_app.h"

#include "gui/background_transcriber.h"
#include "gui/control_channel.h"
#include "gui/dictation.h"
#include "gui/hotkey.h"
#include "gui/layout.h"
#include "gui/overlay.h"
#include "gui/sounds.h"
#include "gui/state.h"
#include "gui/text_insert.h"
#include "gui/theme.h"

#include "history.h"
#include "model_download.h"
#include "model_profiles.h"
#include "settings.h"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

constexpr float kMouseWheelScale = 0.32f;

// Codes for SDL user events posted from background threads.
enum UserCode : Sint32 {
  kHotkeyDown = 1,
  kHotkeyUp = 2,
  kControlToggle = 3,
  kControlShow = 4,
  kControlStart = 5,
  kControlStop = 6,
  kDownloadDone = 7,
};

Uint32 g_user_event = 0;

void post_user_event(Sint32 code) {
  SDL_Event e;
  SDL_zero(e);
  e.type = g_user_event;
  e.user.code = code;
  SDL_PushEvent(&e); // thread-safe
}

bool initialize_window(SDL_Window **window, SDL_Renderer **renderer, bool start_minimized) {
  Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
  if (start_minimized) {
    flags |= SDL_WINDOW_MINIMIZED;
  }
  *window =
      SDL_CreateWindow("Lisper", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1240, 840, flags);
  if (*window == nullptr) {
    std::fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    return false;
  }
  SDL_SetWindowMinimumSize(*window, 900, 620);

  *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (*renderer == nullptr) {
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (*renderer == nullptr) {
    std::fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(*window);
    *window = nullptr;
    return false;
  }
  return true;
}

void damp_mouse_wheel(SDL_Event *event) {
  if (event == nullptr || event->type != SDL_MOUSEWHEEL) {
    return;
  }
  const Sint32 original_x = event->wheel.x;
  const Sint32 original_y = event->wheel.y;
#if SDL_VERSION_ATLEAST(2, 0, 18)
  event->wheel.preciseX *= kMouseWheelScale;
  event->wheel.preciseY *= kMouseWheelScale;
#endif
  event->wheel.x = static_cast<Sint32>(static_cast<float>(event->wheel.x) * kMouseWheelScale);
  event->wheel.y = static_cast<Sint32>(static_cast<float>(event->wheel.y) * kMouseWheelScale);
  if (event->wheel.x == 0 && original_x != 0) {
    event->wheel.x = original_x > 0 ? 1 : -1;
  }
  if (event->wheel.y == 0 && original_y != 0) {
    event->wheel.y = original_y > 0 ? 1 : -1;
  }
}

float detect_content_scale(SDL_Window *window, SDL_Renderer *renderer) {
  int window_w = 0;
  int window_h = 0;
  int output_w = 0;
  int output_h = 0;
  SDL_GetWindowSize(window, &window_w, &window_h);
  SDL_GetRendererOutputSize(renderer, &output_w, &output_h);
  if (window_w <= 0 || window_h <= 0) {
    return 1.0f;
  }
  const float scale_x = static_cast<float>(output_w) / window_w;
  const float scale_y = static_cast<float>(output_h) / window_h;
  return std::max(1.0f, std::min(scale_x, scale_y));
}

void print_gui_help() {
  std::printf("Lisper desktop app\n\n"
              "  lisper_gui              Start the app (or focus the running one)\n"
              "  lisper_gui --toggle     Start/stop dictation in the running app\n"
              "  lisper_gui --start      Start dictation\n"
              "  lisper_gui --stop       Stop dictation and insert the text\n"
              "  lisper_gui --minimized  Start in the background\n\n"
              "Bind --toggle to a system shortcut where global shortcuts are unavailable\n"
              "(for example GNOME/KDE on Wayland).\n");
}

// Clipboard/paste sequencing for one dictation result.
struct PendingInsert {
  bool active = false;
  bool paste = false;
  bool restore = false;
  std::string previous_clipboard;
  Uint64 paste_at = 0;
  Uint64 restore_at = 0;
};

} // namespace

int run_gui_app(int argc, char **argv) {
  bool start_minimized = false;
  gui::Page start_page = gui::Page::Home;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      print_gui_help();
      return 0;
    }
    if (a == "--toggle" || a == "--start" || a == "--stop" || a == "--show") {
      if (gui::ControlChannel::send(a.substr(2))) {
        return 0;
      }
      std::fprintf(stderr, "Lisper is not running. Start lisper_gui first.\n");
      return 1;
    }
    if (a == "--minimized") {
      start_minimized = true;
    }
    if (a == "--page" && i + 1 < argc) {
      const std::string page = argv[++i];
      start_page = page == "transcribe"   ? gui::Page::Transcribe
                   : page == "dictionary" ? gui::Page::Dictionary
                   : page == "snippets"   ? gui::Page::Snippets
                   : page == "style"      ? gui::Page::Style
                   : page == "settings"   ? gui::Page::Settings
                                          : gui::Page::Home;
    }
  }

  // Single instance: a second launch just brings the first one forward.
  gui::ControlChannel control;
  if (!control.listen([](const std::string &cmd) {
        if (cmd == "toggle") {
          post_user_event(kControlToggle);
        } else if (cmd == "show") {
          post_user_event(kControlShow);
        } else if (cmd == "start") {
          post_user_event(kControlStart);
        } else if (cmd == "stop") {
          post_user_event(kControlStop);
        }
      })) {
    return 0;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  g_user_event = SDL_RegisterEvents(1);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
  SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
  SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  if (!initialize_window(&window, &renderer, start_minimized)) {
    SDL_Quit();
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;

  const float content_scale = detect_content_scale(window, renderer);
  SDL_RenderSetScale(renderer, content_scale, content_scale);
  const gui::Fonts fonts = gui::install_fonts(content_scale);
  io.FontGlobalScale = 1.0f / content_scale;

  gui::AppState state;
  state.page = start_page;
  state.settings = settings_store::load();
  gui::apply_theme(state.settings.dark_theme);
  gui::copy_string(state.language, state.settings.language);
  state.threads = gui::recommended_threads();
  state.status_line.clear();
  gui::refresh_resolved_model(state);

  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);

  gui::BackgroundTranscriber job;
  gui::Dictation dictation;
  gui::Overlay overlay;
  gui::GlobalHotkey hotkey;
  dictation.configure(state.settings);

  auto rebind_hotkey = [&] {
    const gui::Hotkey hk = gui::parse_hotkey(state.settings.hotkey);
    state.hotkey_error.clear();
    state.hotkey_registered = hotkey.register_hotkey(
        hk, [](bool pressed) { post_user_event(pressed ? kHotkeyDown : kHotkeyUp); },
        state.hotkey_error);
  };
  rebind_hotkey();

  {
    std::string hint;
    gui::paste_supported(hint);
    state.paste_hint = hint;
  }

  // Model downloads run on a worker; progress is reported via `state`.
  // The worker only touches this shared block, so it can be detached on quit
  // without blocking shutdown on a multi-gigabyte download.
  struct DownloadShared {
    std::mutex mutex;
    std::string result;
  };
  auto download_shared = std::make_shared<DownloadShared>();
  std::thread download_thread;
  auto download_model = [&](const std::string &profile_name) {
    const ModelProfile *profile = find_model_profile(profile_name);
    if (profile == nullptr || state.downloading) {
      return;
    }
    if (download_thread.joinable()) {
      download_thread.join();
    }
    state.downloading = true;
    state.download_status = "Downloading " + profile->filename + "... this can take a few minutes.";
    download_thread = std::thread([shared = download_shared, prof = *profile] {
      std::string error;
      const std::string path = model_download::download_profile(prof, error);
      {
        std::lock_guard<std::mutex> lock(shared->mutex);
        shared->result = path.empty() ? "Download failed: " + error : "Installed " + prof.filename;
      }
      post_user_event(kDownloadDone);
    });
  };

  auto copy_text = [](const std::string &text) { SDL_SetClipboardText(text.c_str()); };

  gui::UiContext ui{state, fonts, job, dictation, rebind_hotkey, download_model, copy_text, [] {
#ifdef __APPLE__
                      gui::request_accessibility_permission();
#endif
                    }};

  PendingInsert pending;
  gui::Dictation::Phase last_phase = gui::Dictation::Phase::Idle;
  Uint64 settings_dirty_at = 0;
  bool done = false;

  auto bring_to_front = [&] {
    SDL_ShowWindow(window);
    SDL_RestoreWindow(window);
    SDL_RaiseWindow(window);
  };

  while (!done) {
    const Uint32 window_flags = SDL_GetWindowFlags(window);
    const bool minimized = (window_flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
    const bool animating = dictation.phase() != gui::Dictation::Phase::Idle || job.running() ||
                           pending.active || state.downloading;

    // Sleep when nothing is happening to keep idle CPU near zero.
    SDL_Event event;
    bool have_event = false;
    if (!animating && (minimized || (window_flags & SDL_WINDOW_INPUT_FOCUS) == 0)) {
      have_event = SDL_WaitEventTimeout(&event, minimized ? 250 : 50) != 0;
    } else {
      have_event = SDL_PollEvent(&event) != 0;
    }

    while (have_event) {
      bool overlay_clicked = false;
      if (overlay.handle_event(event, overlay_clicked)) {
        if (overlay_clicked) {
          dictation.toggle();
        }
      } else if (event.type == g_user_event) {
        switch (event.user.code) {
        case kHotkeyDown:
          if (!state.capturing_hotkey) {
            if (state.settings.hotkey_hold) {
              dictation.start();
            } else {
              dictation.toggle();
            }
          }
          break;
        case kHotkeyUp:
          if (state.settings.hotkey_hold) {
            dictation.stop();
          }
          break;
        case kControlToggle:
          dictation.toggle();
          break;
        case kControlStart:
          dictation.start();
          break;
        case kControlStop:
          dictation.stop();
          break;
        case kControlShow:
          bring_to_front();
          break;
        case kDownloadDone: {
          std::lock_guard<std::mutex> lock(download_shared->mutex);
          state.downloading = false;
          state.download_status = download_shared->result;
          dictation.configure(state.settings);
          break;
        }
        default:
          break;
        }
      } else {
        // Capture a new shortcut in Settings before ImGui sees the key.
        if (state.capturing_hotkey && event.type == SDL_KEYDOWN && !event.key.repeat) {
          if (event.key.keysym.sym == SDLK_ESCAPE) {
            state.capturing_hotkey = false;
          } else {
            const gui::Hotkey hk = gui::hotkey_from_sdl(event.key.keysym.sym, event.key.keysym.mod);
            if (hk.valid()) {
              state.settings.hotkey = hk.to_string();
              state.capturing_hotkey = false;
              state.settings_dirty = true;
              rebind_hotkey();
            }
          }
        } else {
          SDL_Event imgui_event = event;
          damp_mouse_wheel(&imgui_event);
          ImGui_ImplSDL2_ProcessEvent(&imgui_event);
        }

        if (event.type == SDL_QUIT) {
          done = true;
        } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                   event.window.windowID == SDL_GetWindowID(window)) {
          if (state.settings.close_to_background) {
            SDL_MinimizeWindow(window);
          } else {
            done = true;
          }
        } else if (event.type == SDL_DROPFILE) {
          if (event.drop.file != nullptr) {
            gui::copy_string(state.input_path, fs::absolute(event.drop.file).string());
            state.page = gui::Page::Transcribe;
            SDL_free(event.drop.file);
          }
        }
      }
      have_event = SDL_PollEvent(&event) != 0;
    }

    if (state.want_quit) {
      done = true;
    }
    if (done) {
      break;
    }

    // --- dictation lifecycle -------------------------------------------------
    dictation.update();
    const gui::Dictation::Phase phase = dictation.phase();
    if (phase != last_phase && state.settings.sounds) {
      if (phase == gui::Dictation::Phase::Recording) {
        gui::play_chime(gui::Chime::Start);
      } else if (last_phase == gui::Dictation::Phase::Recording &&
                 phase == gui::Dictation::Phase::Transcribing) {
        gui::play_chime(gui::Chime::Stop);
      } else if (phase == gui::Dictation::Phase::Error) {
        gui::play_chime(gui::Chime::Error);
      }
    }
    last_phase = phase;

    if (auto result = dictation.consume_result()) {
      const bool app_focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
      pending = PendingInsert{};
      pending.active = true;
      pending.paste = state.settings.insert_mode == Settings::InsertMode::Paste && !app_focused;
      pending.restore = pending.paste && state.settings.restore_clipboard;
      if (pending.restore) {
        char *prev = SDL_GetClipboardText();
        pending.previous_clipboard = prev != nullptr ? prev : "";
        SDL_free(prev);
      }
      SDL_SetClipboardText(result->text.c_str());
      // Give the user a moment to release the shortcut's modifier keys.
      pending.paste_at = SDL_GetTicks64() + 120;
      if (!pending.paste) {
        dictation.report_inserted(true, app_focused ? "Copied: switch to an app and dictate there "
                                                      "to insert directly"
                                                    : "Copied to clipboard");
        pending.active = false;
        gui::show_toast(state, "Copied to clipboard");
      }
      if (state.settings.save_history) {
        history::append(history::make_entry(result->text, result->raw_text, result->duration_ms,
                                            result->language),
                        state.settings.history_limit);
        state.history_dirty = true;
      }
    }

    if (pending.active) {
      const Uint64 now = SDL_GetTicks64();
      if (pending.paste && now >= pending.paste_at) {
        std::string error;
        const bool ok = gui::send_paste_keystroke(error);
        dictation.report_inserted(ok, ok ? "" : error);
        if (!ok) {
          state.paste_hint = error;
          pending.restore = false; // leave the text on the clipboard
        }
        pending.paste = false;
        pending.restore_at = now + 650;
      }
      if (!pending.paste && (!pending.restore || now >= pending.restore_at)) {
        if (pending.restore) {
          SDL_SetClipboardText(pending.previous_clipboard.c_str());
        }
        pending.active = false;
      }
    }

    // --- background job ------------------------------------------------------
    if (auto completed = job.consume_completed()) {
      state.status_line = completed->status;
      state.preview_text = completed->preview_text;
      state.final_output_path = completed->final_output_path;
      state.detected_language = completed->result.detected_language;
      state.duration_ms = completed->result.duration_ms;
      state.segment_count = static_cast<int>(completed->result.segments.size());
      state.last_run_success = completed->success;
      gui::append_log(state, completed->status);
    }

    // --- persistence -----------------------------------------------------------
    if (state.history_dirty) {
      state.history = history::load(state.settings.history_limit);
      state.stats = history::compute_stats(state.history, history::now_ms());
      state.history_dirty = false;
    }
    if (state.settings_dirty) {
      if (settings_dirty_at == 0) {
        settings_dirty_at = SDL_GetTicks64();
      } else if (SDL_GetTicks64() - settings_dirty_at > 400) {
        settings_store::save(state.settings);
        dictation.configure(state.settings);
        state.settings_dirty = false;
        settings_dirty_at = 0;
      }
    }

    // --- render ---------------------------------------------------------------
    overlay.update(dictation, state.settings.show_overlay, state.settings.dark_theme);

    if (!minimized) {
      ImGui_ImplSDLRenderer2_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();
      gui::render_main_window(ui);
      ImGui::Render();
      const ImVec4 bg = gui::palette().bg;
      SDL_SetRenderDrawColor(renderer, static_cast<Uint8>(bg.x * 255),
                             static_cast<Uint8>(bg.y * 255), static_cast<Uint8>(bg.z * 255), 255);
      SDL_RenderClear(renderer);
      ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
      SDL_RenderPresent(renderer);
    } else if (animating) {
      SDL_Delay(16);
    }
  }

  if (state.settings_dirty) {
    settings_store::save(state.settings);
  }
  hotkey.unregister();
  dictation.cancel();
  job.cancel();
  job.wait();
  if (download_thread.joinable()) {
    if (state.downloading) {
      download_thread.detach(); // curl finishes or is killed with the process
    } else {
      download_thread.join();
    }
  }
  overlay.destroy();
  gui::shutdown_sounds();
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
