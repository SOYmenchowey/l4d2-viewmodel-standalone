// Minimal warm-styled layout: Viewmodel and Tercera persona.
#define ICON_FA_CLOSE "\xef\x80\x8d"
#define ICON_FA_SAVE "\xef\x83\x87"
#define ICON_FA_RESET "\xef\x83\xa2"
#define ICON_FA_KEYBOARD "\xef\x84\x9c"

static ImU32 Rgba(const ImVec4& color) { return ImGui::GetColorU32(color); }
static void Dirty() { g_dirty=true; g_saveError=false; }

static bool TargetToggle(const char* id, const char* label, bool* value, const char* notice=nullptr) {
    ImGui::PushID(id);
    const float box = 28.0f, gap = 10.0f;
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const bool clicked = ImGui::InvisibleButton("##targetToggle", ImVec2(box + gap + textSize.x, box));
    if (clicked) {
        *value = !*value;
        NotificarCambio(notice ? notice : label, *value);
    }
    const ImVec2 p = ImGui::GetItemRectMin();
    const ImVec2 end(p.x + box, p.y + box);
    auto* dl = ImGui::GetWindowDrawList();
    const ImVec4 on(0.55f, 0.86f, 0.72f, 1.0f);
    const ImVec4 offFill(0.105f, 0.113f, 0.125f, 1.0f);
    const ImVec4 offBorder(0.19f, 0.20f, 0.22f, 1.0f);
    dl->AddRectFilled(p, end, Rgba(*value ? on : offFill), 4.0f);
    dl->AddRect(p, end, Rgba(*value ? on : offBorder), 4.0f, 0, 1.0f);
    if (*value) {
        const ImVec2 a(p.x + 7.0f, p.y + 14.5f);
        const ImVec2 b(p.x + 12.0f, p.y + 20.0f);
        const ImVec2 c(p.x + 21.5f, p.y + 9.0f);
        const ImU32 white = Rgba(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        const ImU32 outline = Rgba(ImVec4(0.025f, 0.028f, 0.032f, 1.0f));
        dl->AddLine(a, b, outline, 4.4f);
        dl->AddLine(b, c, outline, 4.4f);
        dl->AddLine(a, b, white, 2.4f);
        dl->AddLine(b, c, white, 2.4f);
    }
    dl->AddText(ImVec2(p.x + box + gap, p.y + (box - ImGui::GetFontSize()) * 0.5f), Rgba(C_TXT), label);
    ImGui::PopID();
    return clicked;
}

static void DrawValueBadge(const char* text, const ImVec2& at, const ImVec2& size, const ImVec4& bg, const ImVec4& fg);
static void ValueBadgeItem(const char* text, const ImVec2& size, const ImVec4& bg, const ImVec4& fg) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    DrawValueBadge(text, at, size, bg, fg);
    ImGui::Dummy(size);
}

static void SectionHeader(const char* text) {
    ImGui::TextDisabled("%s", text);
    ImGui::Dummy(ImVec2(0, 6));
}

static void DrawValueBadge(const char* text, const ImVec2& at, const ImVec2& size, const ImVec4& bg, const ImVec4& fg) {
    const ImVec2 end(at.x + size.x, at.y + size.y);
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(at, end, Rgba(bg), 4.0f);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(at.x + (size.x - ts.x) * 0.5f, at.y + (size.y - ts.y) * 0.5f), Rgba(fg), text);
}

static bool TargetButton(const char* id, const char* label, const ImVec2& size, bool selected=false) {
    ImGui::PushID(id);
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, C_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, C_ACCENT_H);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, C_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_Text, C_BG);
    }
    const bool clicked = ImGui::Button(label, size);
    if (selected) ImGui::PopStyleColor(4);
    ImGui::PopID();
    return clicked;
}

static bool IconButton(const char* id, const char* icon, const char* tooltip, float size=32.0f) {
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, C_PANEL);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f,0.16f,0.16f,1.0f));
    const bool clicked = ImGui::Button(icon, ImVec2(size, size));
    ImGui::PopStyleColor(3);
    ImGui::PopID();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

static bool TrackSlider(float* value, float minimum, float maximum, const char* id, const char* centeredText) {
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 4));
    const ImGuiCol hidden[] = {ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered,
        ImGuiCol_FrameBgActive, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive};
    for (auto item : hidden) ImGui::PushStyleColor(item, ImVec4(0, 0, 0, 0));
    ImGui::SetNextItemWidth(-1);
    const bool changed = ImGui::SliderFloat("##track", value, minimum, maximum, "", ImGuiSliderFlags_NoInput);
    ImGui::PopStyleColor(5); ImGui::PopStyleVar();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    const float left = a.x + 2, right = b.x - 2, y = (a.y + b.y) * 0.5f;
    const float ratio = (*value - minimum) / (maximum - minimum);
    const float x = left + ImClamp(ratio, 0.0f, 1.0f) * (right - left);
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(left, y), ImVec2(right, y), Rgba(C_BORDER), 3.0f);
    if (minimum < 0 && maximum > 0) {
        const float mid = left + ImClamp(-minimum / (maximum - minimum), 0.0f, 1.0f) * (right - left);
        dl->AddLine(ImVec2(mid, y - 5), ImVec2(mid, y + 5), Rgba(C_DIM), 1.0f);
    }
    if (centeredText && centeredText[0]) {
        const ImVec2 ts = ImGui::CalcTextSize(centeredText);
        dl->AddText(ImVec2((a.x + b.x - ts.x) * 0.5f, y - ts.y * 0.5f), Rgba(C_TXT), centeredText);
    }
    dl->AddCircleFilled(ImVec2(x, y), 7.0f, Rgba(C_ACCENT), 16);
    ImGui::PopID();
    return changed;
}

static bool AxisTrack(float* value, float minimum, float maximum, const ImVec4& color, const char* id) {
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 3));
    const ImGuiCol hidden[] = {ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered,
        ImGuiCol_FrameBgActive, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive};
    for (auto item : hidden) ImGui::PushStyleColor(item, ImVec4(0, 0, 0, 0));
    ImGui::SetNextItemWidth(-1);
    const bool changed = ImGui::SliderFloat("##axis", value, minimum, maximum, "", ImGuiSliderFlags_NoInput);
    ImGui::PopStyleColor(5); ImGui::PopStyleVar();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    const float left = a.x + 2, right = b.x - 2, y = (a.y + b.y) * 0.5f;
    const float ratio = (*value - minimum) / (maximum - minimum);
    const float x = left + ImClamp(ratio, 0.0f, 1.0f) * (right - left);
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(left, y), ImVec2(right, y), Rgba(C_BORDER), 3.0f);
    const float mid = left + ImClamp(-minimum / (maximum - minimum), 0.0f, 1.0f) * (right - left);
    dl->AddLine(ImVec2(mid, y - 6), ImVec2(mid, y + 6), Rgba(C_DIM), 1.0f);
    dl->AddCircleFilled(ImVec2(x, y), 7.0f, Rgba(color), 16);
    ImGui::PopID();
    return changed;
}

static bool AxisControl(const char* id, const char* label, const char* icon, const ImVec4& color, volatile float* value) {
    ImGui::PushID(id);
    float current = *value;
    const float contentW = ImGui::GetContentRegionAvail().x;
    const float iconSize = 28.0f, badgeW = 72.0f, rowH = 28.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 iconEnd(p.x + iconSize, p.y + iconSize);
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, iconEnd, Rgba(color), 4.0f);
    const ImVec2 iconSizeCalc = ImGui::CalcTextSize(icon);
    dl->AddText(ImGui::GetFont(), 16.0f,
        ImVec2(p.x + (iconSize - iconSizeCalc.x) * 0.5f, p.y + (iconSize - 16.0f) * 0.5f),
        Rgba(C_BG), icon);
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + iconSize + 12.0f, p.y + (iconSize - labelSize.y) * 0.5f), Rgba(C_TXT), label);
    char valueText[24]; sprintf_s(valueText, "%+.2f", current);
    ImGui::SetCursorPosX(contentW - badgeW);
    ValueBadgeItem(valueText, ImVec2(badgeW, rowH), C_PANEL, C_TXT);
    ImGui::Dummy(ImVec2(0, 2));
    const bool changed = AxisTrack(&current, -20.0f, 20.0f, color, id);
    if (changed) *value = current;
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::PopID();
    return changed;
}

static void BindButton(const char* id, int& key, bool& capturing) {
    ImGui::PushID(id);
    const bool wasCapturing = capturing;
    if (wasCapturing) ImGui::PushStyleColor(ImGuiCol_Text, C_ACCENT);
    const std::string label = wasCapturing ? "..." : key ? NombreTecla(key) : "Sin tecla";
    if (ImGui::Button(label.c_str(), ImVec2(104, 30))) {
        const bool start = !wasCapturing;
        CancelarCaptura();
        capturing = start;
    }
    if (wasCapturing) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tecla ON / OFF. Esc cancela.");
    ImGui::PopID();
}

static void CrosshairPreview() {
    const ImVec2 size(ImGui::GetContentRegionAvail().x, 48.0f);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##crosshairPreview", size);
    auto* dl = ImGui::GetWindowDrawList();
    const ImVec2 end(p.x + size.x, p.y + size.y);
    dl->AddRectFilled(p, end, Rgba(C_BG), 5.0f);
    dl->AddRect(p, end, Rgba(C_BORDER), 5.0f);
    const ImVec2 center(floorf((p.x + end.x) * 0.5f) + 0.5f,
                        floorf((p.y + end.y) * 0.5f) + 0.5f);
    const ULONGLONG now = GetTickCount64();
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(ColorCrosshair(now));
    DibujarFormaCrosshair(dl, center, TipoCrosshair(now), ImMin(g_crosshairTamano, 10.0f),
        ImMin(g_crosshairGrosor, 3.0f), color);
}

static void NativeViewmodel() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 3));
    SectionHeader("JUGADOR / VIEWMODEL");
    if (AxisControl("vmX", "Profundidad", "X", C_AXES[0], &vmh::g_offx)) Dirty();
    if (AxisControl("vmY", "Lateral", "Y", C_AXES[1], &vmh::g_offy)) Dirty();
    if (AxisControl("vmZ", "Altura", "Z", C_AXES[2], &vmh::g_offz)) Dirty();

    ImGui::Dummy(ImVec2(0, 3));
    ImGui::TextDisabled("VIEWMODEL");
    ImGui::Dummy(ImVec2(0, 2));
    if (TargetToggle("vmEnabled", "Activado", &g_vmActivado, "Viewmodel")) {
        vmh::g_factor = g_vmActivado ? 1.0f : 0.0f;
        Dirty();
    }
    ImGui::SameLine(0, 0);
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 104.0f);
    BindButton("vmKey", g_vmHotkey, g_vmCapturando);
    ImGui::PopStyleVar();
}

static void NativeCrosshair() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 5));
    SectionHeader("MIRA / CROSSHAIR");
    if (TargetToggle("crosshair", "Crosshair", &g_crosshairActivado, "Crosshair")) Dirty();
    const char* tipos[] = {"Cruz (+)", "Diagonal (X)", "Punto", "Circulo"};
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("Tipo##crosshairType", &g_crosshairTipo, tipos, 4)) Dirty();

    if (ImGui::ColorEdit3("Color##crosshairColor", (float*)&g_crosshairColor, ImGuiColorEditFlags_NoInputs)) Dirty();
    ImGui::SameLine(0, 8);
    if (TargetToggle("crosshairRGB", "RGB", &g_crosshairRGB, "RGB animado")) Dirty();

    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("Tamaño##crosshairSize", &g_crosshairTamano, 2.0f, 40.0f,
        "%.0f px", ImGuiSliderFlags_AlwaysClamp)) Dirty();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("Grosor##crosshairThickness", &g_crosshairGrosor, 1.0f, 8.0f,
        "%.1f px", ImGuiSliderFlags_AlwaysClamp)) Dirty();

    if (TargetToggle("crosshairAutoShape", "Cambiar forma", &g_crosshairAutoForma, "Alterna las 4 formas cada 0.7 s")) {
        g_crosshairAnimInicio = GetTickCount64();
        Dirty();
    }
    g_crosshairTamano = ImClamp(g_crosshairTamano, 2.0f, 40.0f);
    g_crosshairGrosor = ImClamp(g_crosshairGrosor, 1.0f, 8.0f);
    CrosshairPreview();
    ImGui::PopStyleVar();
}

static void NativeCamera() {
    SectionHeader("TERCERA PERSONA");
    const float maxX = ImGui::GetContentRegionMax().x;
    if (TargetToggle("thirdPerson", "Tercera persona", &tp::g_activado, "Tercera persona")) Dirty();
    ImGui::SameLine(0, 0);
    ImGui::SetCursorPosX(maxX - 104.0f);
    BindButton("tpKey", g_tpHotkey, g_tpCapturando);

    ImGui::Dummy(ImVec2(0, 16));
    ImGui::TextUnformatted("Distancia");
    ImGui::SameLine(0, 0);
    char distanceText[32]; sprintf_s(distanceText, "%.0f", tp::g_distancia);
    ImGui::SetCursorPosX(maxX - 58.0f);
    ValueBadgeItem(distanceText, ImVec2(58.0f, 30.0f), C_PANEL, C_TXT);
    ImGui::Dummy(ImVec2(0, 2));
    if (TrackSlider(&tp::g_distancia, tp::DISTANCIA_MIN, tp::DISTANCIA_MAX, "tpDistance", nullptr)) Dirty();
    tp::g_distancia = tp::LimitarDistancia(tp::g_distancia);

    if (tp::HayError()) {
        ImGui::Spacing();
        ImGui::TextColored(C_AXES[0], "No se pudo aplicar la camara.");
    }
}

static void DrawNative(float alpha, float slide) {
    const auto display = ImGui::GetIO().DisplaySize;
    const ImVec2 size(ImMin(360.0f, ImMax(280.0f, display.x - 16)), ImMin(500.0f, ImMax(300.0f, display.y - 16)));
    if (g_panelPos.x < 0) g_panelPos = ImVec2((display.x - size.x) * 0.5f, (display.y - size.y) * 0.5f);
    g_panelPos.x = ImClamp(g_panelPos.x, 0.0f, ImMax(0.0f, display.x - size.x));
    g_panelPos.y = ImClamp(g_panelPos.y, 0.0f, ImMax(0.0f, display.y - size.y));

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::SetNextWindowPos(ImVec2(g_panelPos.x, g_panelPos.y + slide), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("VM", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##drag", ImVec2(size.x - 46, 46));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        const auto delta = ImGui::GetIO().MouseDelta;
        g_panelPos.x = ImClamp(g_panelPos.x + delta.x, 0.0f, ImMax(0.0f, display.x - size.x));
        g_panelPos.y = ImClamp(g_panelPos.y + delta.y, 0.0f, ImMax(0.0f, display.y - size.y));
    }
    ImGui::SetCursorPos(ImVec2(size.x - 40, 7));
    if (IconButton("close", ICON_FA_CLOSE, "Cerrar", 32)) Cerrar();
    ImGui::SetCursorPos(ImVec2(14, 5));
    ImGui::TextUnformatted("L4D2 Viewmodel");
    ImGui::SetCursorPos(ImVec2(14, 25));
    ImGui::TextDisabled("@tokyossz");

    g_tab = ImClamp(g_tab, 0, 2);
    const float tabsY = 54.0f, tabH = 30.0f;
    const bool compactTabs = size.x < 340.0f;
    const float tabWidths[] = {compactTabs ? 66.0f : 102.0f,
        compactTabs ? 56.0f : 72.0f, compactTabs ? 112.0f : 138.0f};
    const char* tabs[] = {compactTabs ? "VM" : "Viewmodel", "Mira",
        compactTabs ? "3a Persona" : "Tercera persona"};
    ImGui::SetCursorPos(ImVec2(14, tabsY));
    for (int i = 0; i < 3; ++i) {
        if (TargetButton(tabs[i], tabs[i], ImVec2(tabWidths[i], tabH), g_tab == i)) { g_tab = i; CancelarCaptura(); }
        if (i < 2) ImGui::SameLine(0, 8);
    }

    const float footerH = 76.0f;
    const float bodyY = 94.0f;
    const float bodyH = ImMax(80.0f, size.y - bodyY - footerH);
    ImGui::SetCursorPos(ImVec2(14, bodyY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 8));
    ImGui::BeginChild("##options", ImVec2(size.x - 28, bodyH), false, ImGuiWindowFlags_NoScrollbar);
    if (g_tab == 0) NativeViewmodel();
    else if (g_tab == 1) NativeCrosshair();
    else NativeCamera();
    ImGui::EndChild();
    ImGui::PopStyleVar();

    const float bottom = size.y - footerH;
    ImGui::SetCursorPos(ImVec2(14, bottom + 5)); ImGui::Separator();
    ImGui::SetCursorPos(ImVec2(14, bottom + 13));
    if (IconButton("save", ICON_FA_SAVE, "Guardar ajustes", 32)) GuardarConfig();
    ImGui::SameLine(0, 8);
    if (IconButton("resetVm", ICON_FA_RESET, "Restablecer viewmodel", 32)) {
        vmh::g_offx = vmh::g_offy = vmh::g_offz = 0; GuardarConfig();
    }
    ImGui::SameLine(0, 10);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(g_saveError ? C_AXES[0] : g_dirty ? C_AXES[1] : C_DIM, "%s",
        g_saveError ? "Error al guardar" : g_dirty ? "Sin guardar" : g_saved ? "Guardado" : "Preset actual");

    ImGui::SetCursorPos(ImVec2(14, bottom + 44));
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s Menu", ICON_FA_KEYBOARD);
    ImGui::SetCursorPos(ImVec2(size.x - 114, bottom + 44));
    const std::string bindLabel = std::string(g_capturando ? "..." : NombreTecla(g_hotkey)) + "##menuKey";
    if (ImGui::Button(bindLabel.c_str(), ImVec2(100, 28))) {
        const bool start = !g_capturando; CancelarCaptura(); g_capturando = start;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
