#pragma once
#include <imgui.h>
#include <string>

namespace ImGuiExt
{

enum class LabelMode { Inline, Above };

inline bool SliderFloat(const char* label, float* v, float v_min, float v_max,
	LabelMode mode = LabelMode::Above, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	if (mode == LabelMode::Above)
	{
		ImGui::Text("%s", label);
		ImGui::SetNextItemWidth(-FLT_MIN);
		std::string id = std::string("##") + label;
		return ImGui::SliderFloat(id.c_str(), v, v_min, v_max, format, flags);
	}
	return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
}

inline bool SliderInt(const char* label, int* v, int v_min, int v_max,
	LabelMode mode = LabelMode::Above, const char* display_format = "%d")
{
	if (mode == LabelMode::Above)
	{
		ImGui::Text("%s", label);
		ImGui::SetNextItemWidth(-FLT_MIN);
		std::string id = std::string("##") + label;
		return ImGui::SliderInt(id.c_str(), v, v_min, v_max, display_format);
	}
	return ImGui::SliderInt(label, v, v_min, v_max, display_format);
}

inline bool DragFloat3(const char* label, float* v, float v_speed = 1.0f,
	float v_min = 0.0f, float v_max = 0.0f,
	LabelMode mode = LabelMode::Above, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	if (mode == LabelMode::Above)
	{
		ImGui::Text("%s", label);
		ImGui::SetNextItemWidth(-FLT_MIN);
		std::string id = std::string("##") + label;
		return ImGui::DragFloat3(id.c_str(), v, v_speed, v_min, v_max, format, flags);
	}
	return ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
}

inline bool Combo(const char* label, int* current_item, const char* const items[], int items_count,
	LabelMode mode = LabelMode::Above, int popup_max_height_in_items = -1)
{
	if (mode == LabelMode::Above)
	{
		ImGui::Text("%s", label);
		ImGui::SetNextItemWidth(-FLT_MIN);
		std::string id = std::string("##") + label;
		return ImGui::Combo(id.c_str(), current_item, items, items_count, popup_max_height_in_items);
	}
	return ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
}

}
