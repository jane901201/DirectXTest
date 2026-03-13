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
    //static void DrawGrid(ImDrawList* draw_list, )

};


#endif //CMAKEDIRECT3D12GAMEVCPKG_NODEUI_H