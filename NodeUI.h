//
// Created by User on 2026/3/13.
//

#ifndef CMAKEDIRECT3D12GAMEVCPKG_NODEUI_H
#define CMAKEDIRECT3D12GAMEVCPKG_NODEUI_H

#define IMGUI_DEFINE_MATH_OPERATORS
#include<imgui.h>
#include<vector>

namespace editor {
    struct NodePin {
        int id = 0;
    };

    struct Node {
        int id = 0;
        ImVec2 pos = ImVec2(0, 0);
        ImVec2 size = ImVec2(160, 80);
        const char* name = "Node";
    };
}

class NodeUI {
    static void DrawGrid2(ImDrawList* dl, ImVec2 canvas_p0, ImVec2 canvas_sz, ImVec2 pan, float step, ImU32 col) {
        const ImVec2 canvas_p1 = canvas_p0 + canvas_sz;

        // vertical lines
        for (float x = fmodf(pan.x, step); x < canvas_sz.x; x += step)
            dl->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), col);

        // horizontal lines
        for (float y = fmodf(pan.y, step); y < canvas_sz.y; y += step)
            dl->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), col);
    }

    void ShowNodeEditorExample() {

    }
};


#endif //CMAKEDIRECT3D12GAMEVCPKG_NODEUI_H