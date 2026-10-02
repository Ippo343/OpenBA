#pragma once
// code/Game/Player/exe_bodies.h -- declarations of the exe bodies DEFINED in
// code/Game/Player/ (ENDGAME B.3R, block W0-P0c, B3R_RULES R5).
//
// Every body the registry reaches by retail address, declared once, copied
// from its DEFINITION (linkage included), grouped by defining file; the
// retail address(es) it is registered under are the trailing comment. The
// defining .cpp includes this header so the compiler checks each pair.
// Readers call these by name (W1); nothing here changes what runs. Making
// them members is B.4's work.

#include <cstdint>
#include "collection/Collection.h"   // Collection::Array<T> (the overlay record arrays)
#include "Player/Tool.h"             // ToolTileRecord / ToolSegmentRecord
#include "Action/PathAvl.h"           // AvlTreeHead (the AVL sets below)
#include <cstddef>
namespace Video { class Context; }

class PlayerToolManager;      // Game/PlayerToolManager.h
class ComponentDescription;   // Component/ComponentDescription.h
class ToolbarDescription;     // Component/ToolbarDescription.h
namespace UI { class DialogInstance; }
class WorldGeometry;          // Game/WorldGeometry.h
class Map;                    // Game/Map.h
class BridgeTool;             // Player/BridgeTool.h
class FencedTool;             // Player/FencedTool.h
class CreatePlacedDescription;   // Component/CreatePlacedDescription.h
class RadiusDescription;         // Component/RadiusDescription.h
namespace System { class String; }
namespace Math { class Vector3; class Transform; }
struct Box;                   // volume/Volume.h (global, Volume::Box)
struct Rectangle;             // volume/Volume.h (global, Volume::Rectangle)
class PlayerController;       // Game/PlayerController.h
class ComponentEntity;        // Entity/ComponentEntity.h

// ---- bridgetool_validate.cpp
extern "C" int BridgeTool_SnapEndCell_Impl(const BridgeTool* self, int endCell);  // 0x1400fb710
extern "C" uint32_t BridgeTool_ValidatePath_Impl(const BridgeTool* self, int startCell, int endCell, int* outLength);  // 0x1400fb2d0

// ---- keystone_a_removalcalcpath.cpp
extern "C" void* RemovalCalc_helper_8d50(int64_t param_1, void** param_2, int param_3);  // 0x1400f8d50
extern "C" void RemovalCalc_StateMachine_dfa0(int64_t param_1);  // 0x14011dfa0
extern "C" uint64_t RemovalCalc_helper_c700(Map* param_1, int param_2);  // 0x14009c700
extern "C" void RemovalCalc_CitizenAI_9ad0(int64_t param_1, int param_2);  // 0x1400f9ad0

// ---- tool_base_createbar.cpp
void UpdateCreateBar(PlayerToolManager* owner, int width, int height, const System::String* cost, char valid, char hideSize, char single, int multiplier, int startCell, int endCell, const ::Box* box, const Math::Transform* transform);  // 0x1400ffc60
extern "C" void UpdateToolUI_fc60(void* param_1, int param_2, int param_3, void* param_4, char param_5, char param_6, char param_7, int param_8, int param_9, int param_10, void* param_11, void* param_12);  // 0x1400ffc60 B4 FORWARD -> UpdateCreateBar: 1 outside caller (standalone/tool_harness.cpp)
extern "C" uint32_t UpdateToolUI_ebb0(const void* /*never read*/, const ::Box* box, const Math::Transform* transform, Math::Vector3* outCorners);  // 0x1400febb0

// ---- tool_base_drag.cpp
ToolbarDescription** DragTool_FindDescription_5160(const ComponentDescription* description, ToolbarDescription** ret);  // 0x140005160
void* DragTool_FindDescription_5160(uintptr_t descList, void* retval);  // 0x140005160 B4 FORWARD: 1 outside caller (Component/diseasecomponent.cpp)
void DragTool_CloseToolDialog_ff980(PlayerToolManager* owner);  // 0x1400ff980
void DragTool_PlayTickSound_ffbf0(PlayerToolManager* owner);  // 0x1400ffbf0
void DragTool_PlayPlaceSound_1000f0(const void* /*unused*/);  // 0x1401000f0
void DragTool_PlayErrorSound_100140(const void* /*unused*/);  // 0x140100140
void DragPlacementTool_ClearPathEntities_1018d0(const void* /*unused*/, int minCol, int minRow, int maxCol, int maxRow, const ComponentDescription* entityTemplate);  // 0x1401018d0

// ---- tool_base_edit.cpp
ComponentEntity** PlayerEdit_EntityAtCell_a0280(const Map* map, ComponentEntity** retIBP, int cellIndex);  // 0x1400a0280
float PlayerEdit_AvgTerrainHeight_101730(void* , int p2, int p3, int p4, int p5);  // 0x140101730
uint64_t PlayerEdit_FencedAnchorCalc_dbf90(const FencedTool* self, uint32_t* param_2, uint32_t* param_3, uint32_t* param_4, uint32_t* param_5, float* param_6, uint32_t* param_7);  // 0x1400dbf90

// ---- tool_base_highlight.cpp
// The decal tessellation's vertex-dedup set (0x1400ace40): 0x38-byte nodes whose
// 0x18-byte element is the 0x14-byte vertex record plus its vertex index. The
// set orders on the record's first 0x14 bytes, compared per column i (0..3) as
// the short at +2i, the short at +8+2i, then the byte at +0x10+i.
struct DecalVertexKey {
    int16_t  column0[4];   // +0x00 compared as signed shorts
    int16_t  column8[4];   // +0x08 compared as signed shorts
    uint8_t  column10[4];  // +0x10 compared as unsigned bytes
    int32_t  index;        // +0x14 the vertex's index in the slot's vertex list (not compared)
};
static_assert(sizeof(DecalVertexKey) == 0x18, "DecalVertexKey is the 0x18-byte element (0x14001f850 copies six dwords)");
static_assert(offsetof(DecalVertexKey, column8) == 0x08 && offsetof(DecalVertexKey, column10) == 0x10, "DecalVertexKey columns");
static_assert(offsetof(DecalVertexKey, index) == 0x14, "DecalVertexKey::index @ +0x14 (0x1400ace40 reads node+0x14)");
struct DecalVertexNode {
    DecalVertexKey   key;       // +0x00
    DecalVertexNode* parent;    // +0x18
    DecalVertexNode* left;      // +0x20
    DecalVertexNode* right;     // +0x28
    int16_t          balance;   // +0x30
    int16_t          height;    // +0x32
    uint8_t          _pad34[4]; // +0x34
};
static_assert(sizeof(DecalVertexNode) == 0x38, "0x14001f850 MemAlloc(0x38)");
static_assert(offsetof(DecalVertexNode, parent) == 0x18 && offsetof(DecalVertexNode, left) == 0x20, "DecalVertexNode links");
static_assert(offsetof(DecalVertexNode, right) == 0x28 && offsetof(DecalVertexNode, balance) == 0x30, "DecalVertexNode links");
static_assert(offsetof(DecalVertexNode, height) == 0x32, "DecalVertexNode::height @ +0x32");
DecalVertexNode** DecalVertexSetInsert(AvlTreeHead<DecalVertexNode>* set, DecalVertexNode** out, const DecalVertexKey* key);  // 0x14001f6b0
DecalVertexNode* DecalVertexSetFind(const AvlTreeHead<DecalVertexNode>* set, const DecalVertexKey* key);  // 0x14001fc00
extern "C" void TradeWork_helper_14b00(int64_t param_1, void* node);  // 0x140014b00 B4 FORWARD -> Avl38FreeSubtree<Avl38Node>
extern "C" void TradeWork_impl_4b40(int64_t* param_1, int64_t* param_2);  // 0x140014b40 B4 FORWARD -> Avl38Retrace<Avl38Node>
int Player_HighlightAdd_55430(const RadiusDescription* param_1, int param_2);  // 0x140055430
int Player_HighlightAdd_55430(int64_t param_1, int param_2);  // 0x140055430 B4 FORWARD: 1 outside caller (Dialog/radiusui.cpp)

// ---- tool_base_highlight_tess.cpp
extern "C" void TradeWork_impl_ace40(int64_t param_1, int param_2, char param_3, int param_4, int param_5);  // 0x1400ace40

// ---- tool_base_path.cpp
void* PlayerPath_FindComponent_5780(uintptr_t entity, void* retIBP);  // 0x140005780
void* PlayerPath_FindComponent_3ea0(uintptr_t entity, void* retIBP);  // 0x140003ea0
uint64_t PlayerPath_Sub_11e710(uintptr_t param_1);  // 0x14011e710 B4 FORWARD -> DestroyComponent::InRemovalState: 2 outside callers (Component/firecomponent_ignite.cpp, Component/workplacecomponent.cpp)
bool PlayerPath_CharComp_f5d0(Map* param_1, int param_2, int param_3, uint32_t param_4, float param_5, float param_6, float param_7, char param_8, char param_9, const ComponentDescription* param_10);  // 0x14009f5d0
bool PlayerPath_CharComp_f5d0(int* param_1, int param_2, int param_3, uint32_t param_4, float param_5, float param_6, float param_7, char param_8, char param_9, int64_t param_10);  // 0x14009f5d0 B4 FORWARD: 3 outside callers (Component/growthcomponent.cpp, Component/spawncomponent.cpp, Work/foresterwork.cpp)
// 0x140104670 / 0x140104770 / 0x1401048a0 are RemovalTool::CanRemoveBuilding / CanRemoveRoad / CanRemoveField (RemovalTool.h).

// ---- tool_base_place.cpp
// 0x1400fd960 is PlacementTool::FlipVariant (PlacementTool.h).

// ---- tool_base_ray.cpp
extern "C" uint8_t PlayerRay_TerrainMarch_aa530(WorldGeometry* param_1, float* param_2, float* param_3, float* param_4, float* param_5, float* param_6);  // 0x1400aa530
extern "C" uint8_t PlayerRay_GetMouseRay_1005a0(const void* /*never read*/, float* outOrigin, float* outDir);  // 0x1401005a0
extern "C" int PlayerRay_ScreenToCell_101270(const void* /*never read*/);  // 0x140101270
// 0x1400fda00 is PlacementTool::GetPlacement (PlacementTool.h).

// ---- tool_base_render.cpp
extern "C" void HighlightSelect_Impl(void* self);  // 0x1400fc9e0
extern "C" void HighlightClear_Impl(void* self);  // 0x1400fcb40
extern "C" void HighlightRepaint_Impl(void* self);  // 0x14004c4a0

// ---- tool_base_screenrect.cpp
extern "C" uint32_t PlayerScreenRect_BuildCorners_ec70(uint64_t /*never read*/, int param_2, int param_3, int param_4, int param_5, Math::Vector3* param_6);  // 0x1400fec70
extern "C" uint8_t PlayerScreenRect_WorldToScreen_098210(PlayerController* param_1, const Math::Vector3* param_2, float* param_3);  // 0x140098210
extern "C" uint8_t PlayerScreenRect_AccumulateRect_fe960(uint64_t /*never read*/, uint32_t param_2, const Math::Vector3* param_3, struct Rectangle* param_4);  // 0x1400fe960
extern "C" uint8_t PlayerScreenRect_EntityRect_0380(const void* /*never read*/, ComponentEntity* const* param_2, struct Rectangle* param_3);  // 0x140100380
extern "C" uint8_t PlayerScreenRect_Entity_0380(uintptr_t , void* param_2, uint64_t* param_3);  // 0x140100380 B4 FORWARD -> PlayerScreenRect_EntityRect_0380: 1 outside caller (Tutorial/tutorialhighlightdialog.cpp)

// ---- tool_base_tile.cpp
void ToolTile_SetTransformFromYaw_3870(uintptr_t self, const void* offset, float yaw);  // 0x140003870 B4 FORWARD -> ModelComponent::SetTransformFromYaw: 4 outside callers (Component/citizencomponent_lifecycle.cpp, destroycomponent_states.cpp, herdcomponent.cpp, immigrationcomponent_states.cpp)
uint64_t ToolTile_FootprintValidity_d580(Map* param_1, int param_2, int param_3, int param_4, int param_5, unsigned int param_6, const CreatePlacedDescription* param_7, const System::String* param_8, float param_9, float param_10, float param_11, char param_12, const System::String* param_13, int param_14, const ComponentDescription* param_15);  // 0x14009d580
// 0x1400fd650 is PlacementTool::CreateAt (PlacementTool.h).

// ---- tool_base_tilerender.cpp
extern "C" void PlayerTileRender_DrawOverlay_08c0(const void* /*never read*/, Video::Context* ctx, const Collection::Array<ToolTileRecord>* param_3, const Collection::Array<ToolSegmentRecord>* param_4);  // 0x1401008c0
extern "C" void PlayerTileRender_BuildRecords_1380(const void* /*never read*/, Video::Context* ctx, int param_3, int param_4, uint32_t param_5, int param_6, uint32_t param_7, const CreatePlacedDescription* param_8, const System::String* param_9, float param_10, float param_11, float param_12, char param_13, char param_14, const System::String* param_15, int param_16, int param_17, char param_18, const ComponentDescription* param_19);  // 0x140101380

// ---- tool_base_toolui.cpp
extern "C" UI::DialogInstance** ToolUI_OpenToolDialog_ff9e0(PlayerToolManager* owner, UI::DialogInstance** ret, ComponentDescription* description);  // 0x1400ff9e0

// ---- tool_base_trade.cpp
extern "C" void PlayerTrade_Retrace_25920(int64_t* root, int64_t* cursor);  // 0x140025920 B4 FORWARD -> Avl38Retrace<TrackedEntityNode>
extern "C" void PlayerTrade_BSTRemove_25740(int64_t* root, void* nodeArg);  // 0x140025740 B4 FORWARD -> Avl38Remove<TrackedEntityNode>
extern "C" void PlayerTrade_BSTFindRemove_2c690(int64_t* root, void* keyIBP);  // 0x14002c690 B4 FORWARD -> Avl38FindRemove<TrackedEntityNode>
extern "C" void PlayerTrade_EntityUI_66b0(int64_t param_1);  // 0x1400666b0 B4 FORWARD -> EntityUI::SyncPin

// The two rotations of the 0x38-byte AVL nodes (an 0x18-byte element; parent
// +0x18, left +0x20, right +0x28, balance +0x30, height +0x32). Retail's one
// body per rotation serves every such set (TrackedEntityNode, the decal tile
// tree of tool_base_highlight.cpp, TradeWork's, MarketWork's): one template over
// the node (B.4 B13). A node names `parent`, `left`, `right`, `balance`, `height`.
// A child contributes `child->height + 1`, a missing child 0.
template <class Node>
inline int16_t Avl38ChildHeight(const Node* child) {
    return (child == nullptr) ? (int16_t)0 : (int16_t)(child->height + 1);
}

// Recompute one node's balance / height from its children.
template <class Node>
inline void Avl38Update(Node* c) {
    int16_t lh = Avl38ChildHeight(c->left);
    int16_t rh = Avl38ChildHeight(c->right);
    c->balance = (int16_t)(rh - lh);
    c->height = (lh < rh) ? rh : lh;
}

// 0x140015000 — single rotation, pivot = node's LEFT child, which takes node's
// place; node becomes the pivot's RIGHT child. Returns the pivot.
template <class Node>
Node* Avl38RotateRight(Node** root, Node* node) {
    if (node == *root) {
        *root = node->left;
    }
    Node* parent = node->parent;
    if (parent != nullptr) {
        if (parent->left == node) parent->left = node->left;
        else                      parent->right = node->left;
    }
    Node* pivot = node->left;
    pivot->parent = node->parent;
    Node* pivotRight = pivot->right;
    node->left = pivotRight;
    if (pivotRight != nullptr) pivotRight->parent = node;
    pivot->right = node;
    node->parent = pivot;

    // Recompute balance/height for pivot.left, pivot.right, then pivot.
    if (pivot->left != nullptr)  Avl38Update(pivot->left);
    if (pivot->right != nullptr) Avl38Update(pivot->right);
    Avl38Update(pivot);
    return pivot;
}

// 0x140014ed0 — single rotation, pivot = node's RIGHT child, which takes node's
// place; node becomes the pivot's LEFT child. Returns the pivot.
template <class Node>
Node* Avl38RotateLeft(Node** root, Node* node) {
    if (node == *root) {
        *root = node->right;
    }
    Node* parent = node->parent;
    if (parent != nullptr) {
        if (parent->left == node) parent->left = node->right;
        else                      parent->right = node->right;
    }
    Node* pivot = node->right;
    pivot->parent = node->parent;
    Node* pivotLeft = pivot->left;
    node->right = pivotLeft;
    if (pivotLeft != nullptr) pivotLeft->parent = node;
    pivot->left = node;
    node->parent = pivot;

    if (pivot->left != nullptr)  Avl38Update(pivot->left);
    if (pivot->right != nullptr) Avl38Update(pivot->right);
    Avl38Update(pivot);
    return pivot;
}

// 0x140025920 / 0x140014b40 — walk from *cursor to the root recomputing balance /
// height and rotating (two retail copies of one algorithm). *cursor ends null.
template <class Node>
void Avl38Retrace(Node** root, Node** cursor) {
    while (*cursor != nullptr) {
        Node* node = *cursor;
        Node* up = node->parent;  // saved parent (next iteration target)

        int16_t leftH  = Avl38ChildHeight(node->left);
        int16_t rightH = Avl38ChildHeight(node->right);
        int16_t maxH = (rightH < leftH) ? leftH : rightH;
        // balance = rightH - leftH ; height = max(rightH, leftH)
        node->balance = (int16_t)(rightH - leftH);
        node->height = maxH;

        node = *cursor;
        // Right-heavy (bal > 1). Disasm @1400259ad/b8: if right-child bal < 0,
        // CALL 0x140015000 on the right child first, then CALL 0x140014ed0 on node.
        if (node->balance > 1 && node->right != nullptr) {
            Node* rc = node->right;
            if (rc->balance <= 0 && rc->balance < 0) {  // JG/JNS skip => bal_rc < 0
                Avl38RotateRight(root, rc);
                node = *cursor;
            }
            Avl38RotateLeft(root, node);
        }
        node = *cursor;
        // Left-heavy (bal < -1). Disasm @1400259e1/ec: if left-child bal > 0,
        // CALL 0x140014ed0 on the left child first, then CALL 0x140015000 on node.
        if (node->balance < -1 && node->left != nullptr) {
            Node* lc = node->left;
            if (lc->balance >= 0 && lc->balance > 0) {  // JS/JLE skip => bal_lc > 0
                Avl38RotateLeft(root, lc);
                node = *cursor;
            }
            Avl38RotateRight(root, node);
        }
        *cursor = up;
    }
}

// 0x140014b00 — free a subtree: its left (+0x20), its right (+0x28), then itself.
// `context` (RCX) is only passed down.
template <class Node>
void Avl38FreeSubtree(const void* context, Node* node);

// The generic node the unconverted sets of other blocks hand to the C forwarders
// (WORK's TradeWork / MarketWork sets, GAME's / DLG's entity sets): only the
// links and the 8-byte key at +0x00 are read here.
struct Avl38Node {
    const void* key;            // +0x00 compared as an unsigned 64-bit value (0x14002c690)
    uint8_t     _element08[0x10];  // +0x08 the rest of the element (not read)
    Avl38Node*  parent;         // +0x18
    Avl38Node*  left;           // +0x20
    Avl38Node*  right;          // +0x28
    int16_t     balance;        // +0x30
    int16_t     height;         // +0x32
    uint8_t     _pad34[4];      // +0x34
};
static_assert(sizeof(Avl38Node) == 0x38, "Avl38Node is 0x38");
static_assert(offsetof(Avl38Node, parent) == 0x18 && offsetof(Avl38Node, left) == 0x20, "Avl38Node links");
static_assert(offsetof(Avl38Node, right) == 0x28 && offsetof(Avl38Node, balance) == 0x30, "Avl38Node links");
static_assert(offsetof(Avl38Node, height) == 0x32, "Avl38Node::height @ +0x32");
