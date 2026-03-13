//
// Created by User on 2026/3/13.
//

#ifndef CMAKEDIRECT3D12GAMEVCPKG_NODEUI_H
#define CMAKEDIRECT3D12GAMEVCPKG_NODEUI_H

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
        //const ImVec2 canvas_p1 = canvas_p0 + canvas_sz;


    }
};


#endif //CMAKEDIRECT3D12GAMEVCPKG_NODEUI_H