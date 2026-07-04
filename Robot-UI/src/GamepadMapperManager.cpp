#include "GamepadMapperManager.h"
#include "FileManager.h"
#include <cmath>

GamepadMapperManager::GamepadMapperManager()
{
    m_Mappers.reserve(8);
    AddItem();
    m_Mappers[0].isSelected = true;
}

void GamepadMapperManager::AddItem()
{
    GamepadMapper mapper;
    mapper.id = NextId();
    char buf[64];
    snprintf(buf, sizeof(buf), "Item_%d", mapper.id);
    strncpy_s(mapper.name, buf, sizeof(mapper.name) - 1);
    int newIdx = (int)m_Mappers.size();
    m_Mappers.push_back(std::move(mapper));
    m_Mappers[newIdx].isSelected = (newIdx == 0);
    if (newIdx == 0) m_SelectedIndex = 0;
    WL_INFO_TAG("GAMEPAD", "Item added: {} (id={})", m_Mappers[newIdx].name, m_Mappers[newIdx].id);
}

void GamepadMapperManager::RemoveItem(int id)
{
    int index = FindNodeIndex(m_Mappers, id);
    if (index < 0 || index >= (int)m_Mappers.size()) return;
    if (m_Mappers.size() <= 1) return;
    WL_INFO_TAG("GAMEPAD", "Item deleted: {} (id={})", m_Mappers[index].name, m_Mappers[index].id);
    m_Mappers.erase(m_Mappers.begin() + index);
    if (m_SelectedIndex >= (int)m_Mappers.size())
        m_SelectedIndex = (int)m_Mappers.size() - 1;
    if (!m_Mappers.empty()) m_Mappers[m_SelectedIndex].isSelected = true;
}

void GamepadMapperManager::SetSelectedIndex(int idx)
{
    if (idx >= 0 && idx < (int)m_Mappers.size()) {
        for (auto& m : m_Mappers) m.isSelected = false;
        m_SelectedIndex = idx;
        m_Mappers[idx].isSelected = true;
    }
}

void GamepadMapperManager::RenameItem(int id, const char* newName)
{
    for (auto& m : m_Mappers)
        if (m.id == id) { strncpy_s(m.name, newName, sizeof(m.name) - 1); break; }
}

void GamepadMapperManager::DrawContent()
{
    ImGui::Indent(10.0f);
    ImGui::Spacing();
    auto* sel = GetSelectedMapper();
    if (!sel) { ImGui::TextDisabled("No item selected."); ImGui::Unindent(10.0f); return; }

    // ---- Top: gamepad type combo ----
    int typeIdx = (int)sel->gamepad_type;
    if (ImGui::Combo("Gamepad Type", &typeIdx, "Xbox\0Custom\0\0"))
        sel->gamepad_type = (GamepadType)typeIdx;

    sel->RefreshBoundKeys();
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // ---- Top: digital key grid ----
    DrawKeyGrid(*sel, false, avail.y * 0.22f);

    // ---- Canvas: Xbox or Custom ----
    ImGui::BeginChild("##Canvas", ImVec2(0, avail.y * 0.55f), false);

    if (sel->gamepad_type == GamepadType::Xbox)
    {
        auto& image = sel->GetGamepadImageRef();
        if (!image)
            image = std::make_shared<Walnut::Image>(FileManager::GetExeDir() + "..\\..\\..\\asset\\picture\\gamepadmap.png");

        if (!image)
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load gamepad image!");
        else
            DrawXboxCanvas(*sel);
    }
    else
    {
        sel->RebuildCustomKeys();
        DrawCustomCanvas(*sel);
    }

    ImGui::EndChild();

    // ---- Bottom: analog key grid + pending deletes ----
    DrawKeyGrid(*sel, true, 0.0f);
    sel->ProcessPendingDeletes();

    ImGui::Unindent(10.0f);
}

// ============================================================================
// Key grid drawing (digital / analog)
// ============================================================================

void GamepadMapperManager::DrawKeyGrid(GamepadMapper& mapper, bool analog, float height)
{
    const char* sectionLabel = analog ? "ANALOG ACTIONS" : "DIGITAL ACTIONS";
    const char* childId = analog ? "##Analog" : "##Digital";
    const char* ctxId = analog ? "##AnalogCtx" : "##DigitalCtx";
    const char* addLabel = analog ? "Add Analog Key" : "Add Digital Key";
    ImVec4 selectedColor = analog ? ImVec4(0.0f, 0.6f, 1.0f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);

    ImGui::TextDisabled(sectionLabel);
    ImGui::BeginChild(childId, ImVec2(0, height > 0 ? height : 0), true);

    int count = 0;
    for (auto& m : mapper.mappings)
    {
        if (m.is_analog != analog)
            continue;

        ImGui::PushID(m.key_id);

        if (mapper.IsRenaming(m.key_id))
        {
            ImGui::SetNextItemWidth(180);
            bool confirm = ImGui::InputText("##rename", mapper.GetRenameBuffer(), 128,
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            bool canceled = ImGui::IsItemDeactivatedAfterEdit()
                            && !ImGui::IsKeyDown(ImGuiKey_Enter)
                            && !ImGui::IsKeyDown(ImGuiKey_KeypadEnter);
            if (confirm || canceled)
                mapper.EndRename(confirm);
        }
        else
        {
            bool selected = (mapper.GetSelectedKey() == m.key_name);
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Button, selectedColor);
            else if (m.is_bound)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0.5f, 1));

            if (ImGui::Button(m.key_name.c_str(), ImVec2(180, 45)))
                mapper.SetSelectedKey(m.key_name);

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                mapper.BeginRename(m.key_id, m.key_name);

            if (selected || m.is_bound)
                ImGui::PopStyleColor();
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
                mapper.SetPendingDeleteKeyId(m.key_id);
            ImGui::EndPopup();
        }

        ImGui::PopID();

        if (++count % 6 != 0)
            ImGui::SameLine();
    }

    if (ImGui::BeginPopupContextWindow(ctxId,
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem(addLabel))
        {
            int n = 1;
            std::string prefix = analog ? "Axis" : "Key";
            while (true)
            {
                std::string name = prefix + std::to_string(n);
                bool dup = false;
                for (auto& m2 : mapper.mappings)
                    if (m2.key_name == name) { dup = true; break; }
                if (!dup) { mapper.AddKey(name, analog); break; }
                ++n;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::EndChild();
}

// ============================================================================
// Canvas drawing
// ============================================================================

void GamepadMapperManager::DrawXboxCanvas(GamepadMapper& mapper)
{
    auto& image   = mapper.GetGamepadImageRef();
    auto& keys    = mapper.GetXboxPhysicalKeys();
    auto& actions = mapper.GetKeyBoundActions();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float imgW = (float)image->GetWidth();
    float imgH = (float)image->GetHeight();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasTL = ImGui::GetCursorScreenPos();
    ImVec2 canvasSz = avail;

    float scale = std::min(canvasSz.x / imgW, canvasSz.y / imgH);
    ImVec2 renderSz = { imgW * scale, imgH * scale };
    ImVec2 offset = {
        (canvasSz.x - renderSz.x) * 0.5f,
        (canvasSz.y - renderSz.y) * 0.5f
    };
    ImVec2 imgOrigin = { canvasTL.x + offset.x, canvasTL.y + offset.y };

    ImGui::SetCursorScreenPos(imgOrigin);
    ImGui::Image(image->GetDescriptorSet(), renderSz);

    // --- Draw button highlights ---
    for (auto& key : keys)
    {
        if (key.name == "L_Stick_X" || key.name == "L_Stick_Y" ||
            key.name == "R_Stick_X" || key.name == "R_Stick_Y")
            continue;

        float rawVal = mapper.GetRawKeyValue(key.name);
        float val = mapper.CalcActivation(key, rawVal);

        if (val > 0.01f)
        {
            ImVec2 p = {
                imgOrigin.x + key.pos.x * scale,
                imgOrigin.y + key.pos.y * scale
            };
            dl->AddCircleFilled(p,
                key.radius * scale * (1.0f + val * 0.15f),
                IM_COL32(255, 120, 0, (int)(30 + val * 180)));
        }
    }

    // --- Draw sticks ---
    {
        float lx = mapper.GetRawKeyValue("L_Stick_X");
        float ly = mapper.GetRawKeyValue("L_Stick_Y");
        float rx = mapper.GetRawKeyValue("R_Stick_X");
        float ry = mapper.GetRawKeyValue("R_Stick_Y");
        float lMag = std::sqrt(lx * lx + ly * ly);
        float rMag = std::sqrt(rx * rx + ry * ry);
        float stickTravel = 30;

        // Left stick
        {
            ImVec2 center = { imgOrigin.x + 595 * scale, imgOrigin.y + 327 * scale };
            if (lMag > 0.01f)
            {
                dl->AddCircleFilled(center,
                    30 * scale * (1.0f + lMag * 0.15f),
                    IM_COL32(255, 120, 0, (int)(30 + lMag * 180)));
            }
            ImVec2 dot = {
                center.x + lx * stickTravel * scale,
                center.y + ly * stickTravel * scale
            };
            dl->AddCircleFilled(dot, 4 * scale, IM_COL32(255, 60, 40, 220));
        }

        // Right stick
        {
            ImVec2 center = { imgOrigin.x + 857 * scale, imgOrigin.y + 416 * scale };
            if (rMag > 0.01f)
            {
                dl->AddCircleFilled(center,
                    30 * scale * (1.0f + rMag * 0.15f),
                    IM_COL32(255, 120, 0, (int)(30 + rMag * 180)));
            }
            ImVec2 dot = {
                center.x + rx * stickTravel * scale,
                center.y + ry * stickTravel * scale
            };
            dl->AddCircleFilled(dot, 4 * scale, IM_COL32(255, 60, 40, 220));
        }
    }

    // --- Draw bound action labels ---
    float btnHeight = 26 * scale;
    float btnSpacing = 5 * scale;
    float horizontalPadding = 10 * scale;

    for (auto& key : keys)
    {
        auto it = actions.find(key.name);
        if (it == actions.end() || it->second.empty())
            continue;
        auto& boundList = it->second;

        bool isVertical = (key.name == "Button_View" ||
                           key.name == "Button_Menu" ||
                           key.name == "L_Stick_X" ||
                           key.name == "L_Stick_Y" ||
                           key.name == "R_Stick_X" ||
                           key.name == "R_Stick_Y");

        ImVec2 basePos = {
            imgOrigin.x + key.textPos.x * scale,
            imgOrigin.y + key.textPos.y * scale
        };

        if (isVertical)
        {
            float maxWidth = 0;
            for (auto& a : boundList)
            {
                float w = ImGui::CalcTextSize(a.c_str()).x + (horizontalPadding * 2);
                if (w > maxWidth) maxWidth = w;
            }

            float totalHeight = (btnHeight * boundList.size()) +
                                (btnSpacing * (boundList.size() - 1));
            ImVec2 currentPos = {
                basePos.x - (maxWidth * 0.5f),
                basePos.y - (totalHeight * 0.5f)
            };

            for (auto& actionName : boundList)
            {
                ImGui::SetCursorScreenPos(currentPos);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

                std::string label = actionName + "##" + key.name;
                if (ImGui::Button(label.c_str(), ImVec2(maxWidth, btnHeight)))
                    mapper.UnbindKey(actionName);

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
                currentPos.y += btnHeight + btnSpacing;
            }
        }
        else
        {
            float totalWidth = 0;
            std::vector<float> widths;
            for (auto& a : boundList)
            {
                float w = ImGui::CalcTextSize(a.c_str()).x + (horizontalPadding * 2);
                widths.push_back(w);
                totalWidth += w;
            }
            totalWidth += btnSpacing * (boundList.size() - 1);

            ImVec2 startPos = {
                basePos.x - (totalWidth * 0.5f),
                basePos.y - (btnHeight * 0.5f)
            };
            ImGui::SetCursorScreenPos(startPos);

            for (size_t i = 0; i < boundList.size(); ++i)
            {
                if (i > 0)
                    ImGui::SameLine(0, btnSpacing);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

                std::string label = boundList[i] + "##" + key.name;
                if (ImGui::Button(label.c_str(), ImVec2(widths[i], btnHeight)))
                    mapper.UnbindKey(boundList[i]);

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }
        }
    }
}

void GamepadMapperManager::DrawCustomCanvas(GamepadMapper& mapper)
{
    auto& keys    = mapper.GetCustomPhysicalKeys();
    auto& actions = mapper.GetKeyBoundActions();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasTL = ImGui::GetCursorScreenPos();

    for (auto& key : keys)
    {
        ImVec2 p = { canvasTL.x + key.pos.x, canvasTL.y + key.pos.y };
        float rawVal = mapper.GetRawKeyValue(key.name);
        float val = mapper.CalcActivation(key, rawVal);

        ImU32 color;
        if (val > 0.5f)
            color = IM_COL32(255, 120, 0, 220);
        else if (val > 0.01f)
            color = IM_COL32(255, 120, 0, 80);
        else
            color = IM_COL32(80, 80, 80, 120);

        dl->AddCircleFilled(p, key.radius * (1.0f + val * 0.2f), color);
        dl->AddCircle(p, key.radius, IM_COL32(180, 180, 180, 180), 0, 1.5f);

        ImVec2 textPos = {
            canvasTL.x + key.textPos.x,
            canvasTL.y + key.textPos.y - 7
        };
        dl->AddText(textPos, IM_COL32(200, 200, 200, 255), key.label.c_str());

        // Axis value bar
        if (key.is_axis && std::abs(rawVal) > 0.01f)
        {
            float barW = 80;
            float barH = 6;
            ImVec2 barPos = {
                canvasTL.x + key.textPos.x + 60,
                canvasTL.y + key.textPos.y - 3
            };

            dl->AddRectFilled(barPos,
                { barPos.x + barW, barPos.y + barH },
                IM_COL32(50, 50, 50, 200));

            float fillW = std::abs(rawVal) * barW;
            float fillX = (rawVal >= 0)
                ? barPos.x + barW * 0.5f
                : barPos.x + barW * 0.5f - fillW;
            dl->AddRectFilled(
                { fillX, barPos.y },
                { fillX + fillW, barPos.y + barH },
                IM_COL32(255, 140, 40, 220));

            // Center line
            dl->AddRectFilled(
                { barPos.x + barW * 0.5f - 1, barPos.y },
                { barPos.x + barW * 0.5f + 1, barPos.y + barH },
                IM_COL32(200, 200, 200, 180));

            std::string valStr = std::to_string(rawVal).substr(0, 5);
            dl->AddText(
                { barPos.x + barW + 5, barPos.y - 2 },
                IM_COL32(180, 180, 180, 255),
                valStr.c_str());
        }
    }

    // --- Bound action labels ---
    float btnHeight = 22;
    float btnSpacing = 4;

    for (auto& key : keys)
    {
        auto it = actions.find(key.name);
        if (it == actions.end() || it->second.empty())
            continue;
        auto& boundList = it->second;

        ImVec2 basePos = {
            canvasTL.x + key.textPos.x,
            canvasTL.y + key.textPos.y + 16
        };

        for (size_t i = 0; i < boundList.size(); ++i)
        {
            ImGui::SetCursorScreenPos({
                basePos.x,
                basePos.y + i * (btnHeight + btnSpacing)
            });

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 1, 0.5f, 0.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

            std::string label = boundList[i] + "##" + key.name;
            float w = ImGui::CalcTextSize(boundList[i].c_str()).x + 20;
            if (ImGui::Button(label.c_str(), ImVec2(w, btnHeight)))
                mapper.UnbindKey(boundList[i]);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
    }
}

void GamepadMapperManager::ResetToDefault()
{
    m_Mappers.clear();
    m_SelectedIndex = 0;
    GamepadMapper mapper;
    mapper.id = 1;
    strncpy_s(mapper.name, "Default", sizeof(mapper.name) - 1);
    mapper.isSelected = true;
    m_Mappers.push_back(std::move(mapper));
}

GamepadMapper* GamepadMapperManager::GetSelectedMapper()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Mappers.size())
        return &m_Mappers[m_SelectedIndex];
    return nullptr;
}

std::vector<GamepadMapper> GamepadMapperManager::GetAllItems() const
{
    std::vector<GamepadMapper> out;
    for (const auto& m : m_Mappers)
        out.push_back(m);
    return out;
}

void GamepadMapperManager::LoadMappers(const std::vector<GamepadMapper>& items, int selectedIdx)
{
    m_Mappers.clear();
    ResetNextId(1);
    for (const auto& item : items) {
        GamepadMapper mapper;
        mapper.id = NextId();
        strncpy_s(mapper.name, item.name, sizeof(mapper.name) - 1);
        mapper.keys = item.keys;
        mapper.mappings = item.mappings;
        mapper.gamepad_type = item.gamepad_type;
        mapper.UpdateNextKeyID();  // 同步 key id 计数器
        m_Mappers.push_back(std::move(mapper));
    }
    SetSelectedIndex(selectedIdx >= 0 && selectedIdx < (int)m_Mappers.size() ? selectedIdx : 0);
}

void GamepadMapperManager::LoadItems(const std::vector<GamepadMapper>& items)
{
    m_Mappers.clear();
    ResetNextId(1);
    for (const auto& item : items) {
        GamepadMapper mapper;
        mapper.id = NextId();
        strncpy_s(mapper.name, item.name, sizeof(mapper.name) - 1);
        mapper.keys = item.keys;
        mapper.mappings = item.mappings;
        mapper.gamepad_type = item.gamepad_type;
        mapper.UpdateNextKeyID();  // 同步 key id 计数器
        m_Mappers.push_back(std::move(mapper));
    }
    if (m_SelectedIndex >= (int)m_Mappers.size()) m_SelectedIndex = 0;
    if (!m_Mappers.empty()) m_Mappers[m_SelectedIndex].isSelected = true;
}


std::string GamepadMapperManager::ClipboardCopySelected()
{
    int idx = m_SelectedIndex;
    if (idx < 0 || idx >= (int)m_Mappers.size()) return {};
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "type"  << YAML::Value << "gamepad";
    out << YAML::Key << "name"  << YAML::Value << m_Mappers[idx].name;
    out << YAML::Key << "index" << YAML::Value << idx;
    out << YAML::EndMap;
    return out.c_str();
}

void GamepadMapperManager::ClipboardPaste(const std::string& yaml)
{
    try {
        YAML::Node root = YAML::Load(yaml);
        if (!root.IsMap()) return;
        std::string type = root["type"] ? root["type"].as<std::string>() : "";
        int srcIdx = root["index"] ? root["index"].as<int>() : -1;
        if (type != "gamepad" || srcIdx < 0 || srcIdx >= (int)m_Mappers.size()) return;

        std::string baseName = root["name"] ? root["name"].as<std::string>() : "Pasted";
        int n = 1;
        std::string finalName = baseName;
        while (true) {
            bool dup = false;
            for (auto& c : m_Mappers)
                if (std::string(c.name) == finalName) { dup = true; break; }
            if (!dup) break;
            finalName = baseName + "_" + std::to_string(n++);
        }

        AddItem();
        auto& dst = m_Mappers.back();
        strncpy_s(dst.name, sizeof(dst.name), finalName.c_str(), sizeof(dst.name) - 1);
        auto& src = m_Mappers[srcIdx];
        dst.keys = src.keys;
        dst.mappings = src.mappings;
        dst.gamepad_type = src.gamepad_type;
        dst.UpdateNextKeyID();
    } catch (...) {}
}
