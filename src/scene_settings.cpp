/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */
#include "scene_settings.h"
#include "audio.h"
#include "bitmap.h"
#include "input.h"
#include "game_system.h"
#include "cache.h"
#include "input_buttons.h"
#include "input_source.h"
#include "keys.h"
#include "main_data.h"
#include "options.h"
#include "player.h"
#include "baseui.h"
#include "output.h"
#include "utils.h"
#include "scene_language.h"
#include "scene_end.h"
#include "window_about.h"
#include "window_command_horizontal.h"
#include "window_help.h"
#include "window_input_settings.h"
#include "window_numberinput.h"
#include "window_selectable.h"
#include "window_settings.h"
#include <memory>

#ifdef EMSCRIPTEN
#  include <emscripten.h>
#endif

Scene_Settings::Scene_Settings() {
	Scene::type = Scene::Settings;
}

void Scene_Settings::CreateTitleGraphic() {
	// Load Title Graphic
	if (lcf::Data::system.title_name.empty()) {
		return;
	}
	title = std::make_unique<Sprite>();
	FileRequestAsync* request = AsyncHandler::RequestFile("Title", lcf::Data::system.title_name);
	request->SetGraphicFile(true);
	request_id = request->Bind(&Scene_Settings::OnTitleSpriteReady, this);
	request->Start();
}

void Scene_Settings::CreateMainWindow() {
	root_options.clear();
	root_options.insert(root_options.end(), {
		{ Window_Settings::eVideo,	"Hình ảnh" },
		{ Window_Settings::eAudio,	"Âm thanh" },
		{ Window_Settings::eInput,	"Điều khiển"},
		{ Window_Settings::eEngine,	"Phần mềm"},
		{ Window_Settings::eLicense,"Giấy phép"},
		{ Window_Settings::eSave,	"<Lưu cài đặt>"}
	});

	if (Player::translation.HasTranslations() && Scene::Peek()->type != Scene::Title && Scene::Peek()->type != Scene::LanguageMenu) {
		root_options.insert(root_options.begin() + 3, { Window_Settings::eLanguage, "Ngôn ngữ" });
	}

	if (Scene::Find(Scene::Title)) {
		root_options.insert(root_options.end(), { Window_Settings::eEnd, "<Thoát game>" });
	}

	std::vector<std::string> options;
	options.reserve(root_options.size());
	std::for_each(root_options.begin(), root_options.end(), [&](std::pair<Window_Settings::UiMode, std::string> v) { options.emplace_back(v.second); });

	main_window = std::make_unique<Window_Command>(std::move(options));
	main_window->SetHeight(176);
	main_window->SetY((Player::screen_height - main_window->GetHeight()) / 2);
	main_window->SetX((Player::screen_width - main_window->GetWidth()) / 2);

	if (Player::no_audio_flag) {
		main_window->SetItemEnabled(1, !Player::no_audio_flag);
	}
#ifndef SUPPORT_AUDIO
	main_window->DisableItem(1);
#endif
}

void Scene_Settings::CreateOptionsWindow() {
	help_window = std::make_unique<Window_Help>(Player::menu_offset_x, 0, MENU_WIDTH, 32);
	help_window->SetAnimation(Window_Help::Animation::Loop);
	options_window = std::make_unique<Window_Settings>(Player::menu_offset_x + 32, 32, MENU_WIDTH - 64, Player::screen_height - 32 * 2);
	options_window->SetHelpWindow(help_window.get());

	help_window2 = std::make_unique<Window_Help>(Player::menu_offset_x, options_window->GetBottomY(), MENU_WIDTH, 32);
	help_window2->SetAnimation(Window_Help::Animation::Loop);
	options_window->help_window2 = help_window2.get();

	input_window = std::make_unique<Window_InputSettings>(Player::menu_offset_x, 32, MENU_WIDTH, Player::screen_height - 32 * 3);
	input_window->SetHelpWindow(help_window.get());

	std::vector<std::string> input_mode_items = {"Thêm", "Xoá", "Đặt lại"};
	input_mode_window = std::make_unique<Window_Command_Horizontal>(input_mode_items, MENU_WIDTH - 32 * 2);
	input_mode_window->SetX(Player::menu_offset_x + 32);
	input_mode_window->SetY(Player::screen_height - 32);
	input_mode_window->SetHelpWindow(help_window.get());
	input_mode_window->UpdateHelpFn = [](Window_Help& win, int index) {
		if (index == 0) {
			win.SetText("Thêm một phím mới");
		} else if (index == 1) {
			win.SetText("Xoá một phím");
		} else if (index == 2) {
			win.SetText("Đặt lại cài đặt phím về mặc định");
		}
	};

	input_help_window = std::make_unique<Window_Help>(Player::menu_offset_x, Player::screen_height - 64, MENU_WIDTH, 32);

	about_window = std::make_unique<Window_About>(Player::menu_offset_x, Player::menu_offset_y + 32, MENU_WIDTH, MENU_HEIGHT - 64);
	about_window->Refresh();
}

void Scene_Settings::Start() {
	CreateTitleGraphic();
	CreateMainWindow();
	CreateOptionsWindow();

	options_window->Push(Window_Settings::eMain);
	SetMode(Window_Settings::eMain);
}

void Scene_Settings::SetMode(Window_Settings::UiMode new_mode) {
	if (new_mode == mode) {
		return;
	}
	mode = new_mode;

	main_window->SetActive(false);
	main_window->SetVisible(false);
	options_window->SetActive(false);
	options_window->SetVisible(false);
	input_window->SetActive(false);
	input_window->SetVisible(false);
	input_mode_window->SetActive(false);
	input_mode_window->SetVisible(false);
	input_help_window->SetVisible(false);
	help_window->SetVisible(false);
	help_window2->SetVisible(false);
	about_window->SetVisible(false);

	picker_window.reset();
	font_size_window.reset();

	switch (mode) {
		case Window_Settings::eNone:
		case Window_Settings::eMain:
			main_window->SetActive(true);
			main_window->SetVisible(true);
			break;
		case Window_Settings::eInputButtonOption:
			help_window->SetVisible(true);
			input_window->SetVisible(true);
			input_window->SetInputButton(static_cast<Input::InputButton>(options_window->GetFrame().arg));
			input_window->SetIndex(-1);
			input_mode_window->SetActive(true);
			input_mode_window->SetVisible(true);
			input_help_window->SetVisible(true);
			input_help_window->SetText("Đặt lại khẩn cấp: Giữ 4 phím và làm theo hướng dẫn");
			RefreshInputActionAllowed();
			break;
		case Window_Settings::eInputButtonAdd:
			help_window->SetVisible(true);
			input_window->SetVisible(true);
			input_window->SetInputButton(static_cast<Input::InputButton>(options_window->GetFrame().arg));
			input_mode_window->SetVisible(true);
			input_help_window->SetVisible(true);
			input_help_window->SetText("Nhấn một phím bất kì để gán. Để hủy thao tác gán phím, hãy chờ 3 giây");
			break;
		case Window_Settings::eInputButtonRemove:
			help_window->SetVisible(true);
			input_window->SetActive(true);
			input_window->SetVisible(true);
			input_window->SetIndex(0);
			input_mode_window->SetVisible(true);
			input_help_window->SetVisible(true);
			input_help_window->SetText("Chọn một phím bạn muốn xoá");
			break;
		case Window_Settings::eAbout:
			about_window->SetVisible(true);
			break;
		default:
			help_window->SetVisible(true);
			options_window->SetActive(true);
			options_window->SetVisible(true);
			break;
	}
}

void Scene_Settings::Refresh() {
	options_window->Refresh();
}

void Scene_Settings::vUpdate() {
	if (RefreshInputEmergencyReset()) {
		return;
	}

	main_window->Update();
	help_window->Update();
	help_window2->Update();
	options_window->Update();
	input_window->Update();
	input_mode_window->Update();

	auto opt_mode = options_window->GetMode();

	SetMode(opt_mode);

	if (Input::IsTriggered(Input::CANCEL) && opt_mode != Window_Settings::eInputButtonAdd) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Cancel));

		if (number_window) {
			number_window.reset();
			auto& option = options_window->GetCurrentOption();
			option.current_value = option.original_value;
			option.action();
			options_window->SetActive(true);
			return;
		}

		if (picker_window) {
			picker_window.reset();
			auto& option = options_window->GetCurrentOption();
			option.current_value = option.original_value;
			option.action();
			options_window->SetActive(true);
			return;
		}

		help_window2->SetFont(nullptr);
		options_window->Pop();
		SetMode(options_window->GetMode());
		if (mode == Window_Settings::eNone) {
			Scene::Pop();
		}
	}

	switch (opt_mode) {
		case Window_Settings::eNone:
		case Window_Settings::eSave:
		case Window_Settings::eEnd:
		case Window_Settings::eAbout:
		case Window_Settings::eLanguage: // fix compiler warning, not implemented
			break;
		case Window_Settings::eMain:
			UpdateMain();
			break;
		case Window_Settings::eInput:
		case Window_Settings::eVideo:
		case Window_Settings::eAudio:
		case Window_Settings::eAudioMidi:
		case Window_Settings::eAudioSoundfont:
		case Window_Settings::eLicense:
		case Window_Settings::eEngine:
		case Window_Settings::eInputButtonCategory:
		case Window_Settings::eInputListButtonsGame:
		case Window_Settings::eInputListButtonsEngine:
		case Window_Settings::eInputListButtonsDeveloper:
			UpdateOptions();
			break;
		case Window_Settings::eEngineFont1:
			UpdateFont(false);
			break;
		case Window_Settings::eEngineFont2:
			UpdateFont(true);
			break;
		case Window_Settings::eInputButtonOption:
			UpdateButtonOption();
			break;
		case Window_Settings::eInputButtonAdd:
			UpdateButtonAdd();
			break;
		case Window_Settings::eInputButtonRemove:
			UpdateButtonRemove();
			break;
		case Window_Settings::eLastMode:
			assert(false);
	}
}

void Scene_Settings::OnTitleSpriteReady(FileRequestResult* result) {
	BitmapRef bitmapRef = Cache::Title(result->file);

	title->SetBitmap(bitmapRef);

	// If the title sprite doesn't fill the screen, center it to support custom resolutions
	if (bitmapRef->GetWidth() < Player::screen_width) {
		title->SetX(Player::menu_offset_x);
	}
	if (bitmapRef->GetHeight() < Player::screen_height) {
		title->SetY(Player::menu_offset_y);
	}
}

void Scene_Settings::UpdateMain() {

	if (Input::IsTriggered(Input::DECISION)) {
		auto idx = main_window->GetIndex();

		if (main_window->IsItemEnabled(idx)) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Decision));
		} else {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Buzzer));
			return;
		}

		auto mode = root_options[idx].first;

		if (mode == Window_Settings::eLanguage) {
			Scene::Push(std::make_shared<Scene_Language>());
			return;
		} if (mode == Window_Settings::eSave) {
			SaveConfig();
			return;
		} else if (mode == Window_Settings::eEnd) {
			if (Scene::Find(Scene::GameBrowser)) {
				Scene::Push(std::make_unique<Scene_End>(Scene::GameBrowser));
			} else {
				Scene::Push(std::make_unique<Scene_End>(Scene::Null));
			}
			return;
		}

		SetMode(mode);
		options_window->Push(mode);
	}
}

void Scene_Settings::UpdateOptions() {
	if (number_window) {
		number_window->Update();
		auto& option = options_window->GetCurrentOption();
		option.current_value = Utils::Clamp(number_window->GetNumber(), option.min_value, option.max_value);
		option.action();

		if (Input::IsTriggered(Input::DECISION)) {
			options_window->Refresh();
			number_window.reset();
			options_window->SetActive(true);
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Decision));
		}
		return;
	} else if (picker_window) {
		picker_window->Update();
		auto& option = options_window->GetCurrentOption();
		option.current_value = option.options_index[picker_window->GetIndex()];
		option.action();

		if (Input::IsTriggered(Input::DECISION)) {
			options_window->Refresh();
			picker_window.reset();
			options_window->SetActive(true);
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Decision));
		}
		return;
	}

	if (Input::IsTriggered(Input::DECISION)) {
		if (options_window->IsCurrentActionEnabled()) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Decision));
			auto& option = options_window->GetCurrentOption();
			if (option.mode == Window_Settings::eOptionNone) {
				option.action();
				options_window->Refresh();
			} else if (option.mode == Window_Settings::eOptionRangeInput) {
				number_window.reset(new Window_NumberInput(0, 0, 128, 32));
				number_window->SetNumber(option.current_value);
				number_window->SetMaxDigits(std::log10(option.max_value) + 1);
				number_window->SetX(options_window->GetX() + options_window->GetWidth() / 2 - number_window->GetWidth() / 2);
				number_window->SetY(options_window->GetY() + options_window->GetHeight() / 2 - number_window->GetHeight() / 2);
				number_window->SetZ(options_window->GetZ() + 1);
				number_window->SetOpacity(255);
				number_window->SetActive(true);
				help_window->SetText(fmt::format("Nhập một giá trị từ {} đến {}", option.min_value, option.max_value));
				options_window->SetActive(false);
			} else if (option.mode == Window_Settings::eOptionPicker) {
				picker_window.reset(new Window_Command(option.options_text));
				picker_window->SetX(options_window->GetX() + options_window->GetWidth() / 2 - picker_window->GetWidth() / 2);
				picker_window->SetY(options_window->GetY() + options_window->GetHeight() / 2 - picker_window->GetHeight() / 2);
				picker_window->SetZ(options_window->GetZ() + 1);
				picker_window->SetIndex(option.current_value);
				picker_window->SetHelpWindow(help_window.get());
				picker_window->SetActive(true);
				options_window->SetActive(false);
				picker_window->UpdateHelpFn = [this](Window_Help& win, int index) {
					win.SetText(options_window->GetCurrentOption().options_help[index]);
				};
			}
		} else {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Buzzer));
		}
	}

	if (Input::IsTriggered(Input::LEFT) || Input::IsRepeated(Input::LEFT)) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Cursor));
		if (options_window->IsCurrentActionEnabled()) {
			auto& option = options_window->GetCurrentOption();
			if (option.mode == Window_Settings::eOptionRangeInput) {
				--option.current_value;
				if (option.current_value < option.min_value) {
					option.current_value = option.max_value;
				}
				option.action();
			} else if (option.mode == Window_Settings::eOptionPicker) {
				auto it = std::find(option.options_index.begin(), option.options_index.end(), option.current_value);
				assert(it != option.options_index.end());

				if (it == option.options_index.begin()) {
					it = std::prev(option.options_index.end());
				} else {
					std::advance(it, -1);
				}
				option.current_value = *it;
			}
			option.action();
			options_window->Refresh();
		}
	}

	if (Input::IsTriggered(Input::RIGHT) || Input::IsRepeated(Input::RIGHT)) {
		Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Cursor));
		if (options_window->IsCurrentActionEnabled()) {
			auto& option = options_window->GetCurrentOption();
			if (option.mode == Window_Settings::eOptionRangeInput) {
				++option.current_value;
				if (option.current_value > option.max_value) {
					option.current_value = option.min_value;
				}
				option.action();
			} else if (option.mode == Window_Settings::eOptionPicker) {
				auto it = std::find(option.options_index.begin(), option.options_index.end(), option.current_value);
				assert(it != option.options_index.end());

				std::advance(it, 1);
				if (it == option.options_index.end()) {
					it = option.options_index.begin();
				}
				option.current_value = *it;
			}
			option.action();
			options_window->Refresh();
		}
	}
}

void Scene_Settings::UpdateFont(bool mincho) {
	auto fs = Game_Config::GetFontFilesystem();

	auto& last_index = options_window->GetFrame().scratch;

	if (font_size_window) {
		font_size_window->SetY(options_window->GetY() + options_window->GetCursorRect().y);
		font_size_window->Update();
	}

	int index = options_window->GetIndex();
	if (last_index == index) {
		if (index == 0 || !help_window2->GetFont() || help_window2->GetFont()->GetCurrentStyle().size == options_window->font_size.Get()) {
			// Same index or font size did not change
			UpdateOptions();
			return;
		}
	}
	last_index = index;

	if (!font_size_window) {
		font_size_window = std::make_unique<Window_Help>(options_window->GetRightX(), 0, 32, 32);
		font_size_window->SetLeftArrow(true);
		font_size_window->SetRightArrow(true);
		font_size_window->SetAnimateArrows(true);
	}

	font_size_window->SetVisible(false);
	font_size_window->SetText(std::to_string(options_window->font_size.Get()));

	if (index == 0) {
		// Built-In font
		help_window2->Clear();
		help_window2->SetFont(Font::DefaultBitmapFont(mincho));
		help_window2->SetVisible(true);
	} else if (index >= options_window->GetRowMax() - 2) {
		// Sample or browse
		help_window2->Clear();
		help_window2->SetFont(nullptr);
		help_window2->SetVisible(true);
	} else {
		auto is = fs.OpenInputStream(options_window->GetCurrentOption().text);
		if (is) {
			auto font = Font::CreateFtFont(std::move(is), options_window->font_size.Get(), false, false);
			if (font) {
				help_window2->Clear();
				help_window2->SetFont(font);
				help_window2->SetVisible(true);
				font_size_window->SetVisible(true);
			} else {
				auto& opt = options_window->GetCurrentOption();
				opt.action = nullptr;
				opt.value_text = "[Hỏng]";
				opt.help2.clear();
				options_window->DrawOption(options_window->GetIndex());
			}
		}
	}

	UpdateOptions();
}

void Scene_Settings::UpdateButtonOption() {
	if (Input::IsTriggered(Input::DECISION)) {
		switch (input_mode_window->GetIndex()) {
			case 0:
				if (!input_mode_window->IsItemEnabled(0)) {
					Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Buzzer));
				} else {
					Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Decision));
					options_window->Push(Window_Settings::eInputButtonAdd, options_window->GetFrame().arg);
				}
				break;
			case 1:
				if (!input_mode_window->IsItemEnabled(1)) {
					Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Buzzer));
				} else {
					Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Decision));
					options_window->Push(Window_Settings::eInputButtonRemove, options_window->GetFrame().arg);
				}
				break;
			case 2:
				Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Decision));
				input_window->ResetMapping();
				break;
		}
		RefreshInputActionAllowed();
	}
}

void Scene_Settings::UpdateButtonAdd() {
	const Input::KeyStatus* keys = &Input::GetAllRawPressed();

	auto& frame = options_window->GetFrame();
	int& started = frame.scratch;
	int& cancel_timer = frame.scratch2;

	if (cancel_timer == Game_Clock::GetTargetGameFps() * 3) {
		options_window->Pop();
		input_window->Refresh();
		return;
	}
	++cancel_timer;

	// Delay button reading on startup until 0 keys are pressed
	// Prevents that CONFIRM is directly detected as pressed key
	// on some platforms
	if (started == 0) {
		keys = &Input::GetAllRawPressed();
		if (keys->count() != 0) {
			return;
		}
		started = 1;
	}

	for (size_t i = 0; i < keys->size(); ++i) {
		if ((*keys)[i]) {
			auto button = static_cast<Input::InputButton>(frame.arg);
			auto& mappings = Input::GetInputSource()->GetButtonMappings();
			mappings.Add({button, static_cast<Input::Keys::InputKey>(i) });
			options_window->Pop();
			input_window->Refresh();
			break;
		}
	}
}

void Scene_Settings::UpdateButtonRemove() {
	if (Input::IsTriggered(Input::DECISION)) {
		if (input_window->RemoveMapping()) {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Decision));
			options_window->Pop();
		} else {
			Main_Data::game_system->SePlay(Main_Data::game_system->GetSystemSE(Game_System::SFX_Buzzer));
		}
	}
}

bool Scene_Settings::RefreshInputEmergencyReset() {
	if (Input::GetAllRawPressed().count() >= 4) {
		if (input_reset_counter == 0) {
			Output::InfoStr("Đặt lại khẩn cấp các phím điều khiển đã bắt đầu");
			Output::InfoStr("Giữ các phím trong 3 giây");
		}
		input_reset_counter++;

		if (input_reset_counter == Game_Clock::GetTargetGameFps() * 3) {
			if (input_window->GetInputButton() == Input::InputButton::BUTTON_COUNT) {
				// No last button yet: reset everything
				Output::InfoStr("Toàn bộ các phím đã được đặt lại");
				if (input_window->GetActive()) {
					input_window->SetIndex(0);
				}
				Input::ResetAllMappings();
			} else {
				Output::Info("Phím {} đã được đặt lại", Input::kInputButtonNames.tag(input_window->GetInputButton()));
				Output::Info("Để đặt lại tất cả phím hãy giữ thêm 3 giây");
				if (input_window->GetActive()) {
					input_window->SetIndex(0);
				}
				input_window->ResetMapping();
			}
		} else if (input_reset_counter == Game_Clock::GetTargetGameFps() * 6) {
			Output::InfoStr("Toàn bộ các phím đã được đặt lại");
			if (input_window->GetActive()) {
				input_window->SetIndex(0);
			}
			Input::ResetAllMappings();
		}
	} else {
		if (input_reset_counter > 0) {
			Output::InfoStr("Quá trình đặt lại khẩn cấp đã kết thúc");
			input_reset_counter = 0;
		}
	}

	return input_reset_counter > 0;
}

void Scene_Settings::RefreshInputActionAllowed() {
	auto button = static_cast<Input::InputButton>(options_window->GetFrame().arg);
	auto mapping_count =  Input::GetInputSource()->GetButtonMappings().Count(button);
	input_mode_window->SetItemEnabled(0, mapping_count < Window_InputSettings::mapping_limit);
	input_mode_window->SetItemEnabled(1, mapping_count > (Input::IsProtectedButton(button) ? 1 : 0));
}

bool Scene_Settings::SaveConfig(bool silent) {
	auto cfg_out = Game_Config::GetGlobalConfigFileOutput();

	if (!cfg_out) {
		if (silent) {
			Output::Debug("Saving configuration file failed!");
		} else {
			Output::Warning("Không thể lưu tệp tin cài đặt!");
		}
		return false;
	}

	Game_Config cfg;
	cfg.video = DisplayUi->GetConfig();
	cfg.audio = Audio().GetConfig();
	cfg.input = Input::GetInputSource()->GetConfig();
	cfg.player = Player::player_config;

	cfg.WriteToStream(cfg_out);

	AsyncHandler::SaveFilesystem();

	if (silent) {
		Output::Debug("Configuration saved to {}", cfg_out.GetName());
	} else {
		Output::Info("Đã lưu cài đặt vào tệp tin {}", cfg_out.GetName());
	}

	return true;
}
