#include "scene.h"

#include "WireStandardMesh.h"
#include "WireVertexBuffer.h"

using namespace Wire;

namespace BeatScene {

static Node *AddCube(Node *parent, const Char *name, const Color32 &color,
                     const Vector3F &position, const Vector3F &scale) {
    RenderObject *mesh = StandardMesh::CreateCube8(4);
    VertexBuffer *vertices = mesh->GetMesh()->GetVertexBuffer();
    for (UInt i = 0; i < vertices->GetQuantity(); ++i) {
        vertices->Color4(i) = color;
    }

    Node *object = WIRE_NEW Node(mesh);
    object->SetName(String(name));
    object->Local.SetScale(scale);
    object->Local.SetTranslate(position);
    parent->AttachChild(object);
    return object;
}

Node *CreateDefaultEnvironment() {
    Node *scene = WIRE_NEW Node;
    scene->SetName(String("WiiSaberEnvironment"));

    AddCube(scene, "Floor", Color32(31, 36, 56, 255),
            Vector3F(0.0F, -2.2F, -10.0F), Vector3F(4.5F, 0.12F, 14.0F));
    AddCube(scene, "LeftRail", Color32(25, 140, 255, 255),
            Vector3F(-4.2F, 0.0F, -10.0F), Vector3F(0.12F, 2.4F, 14.0F));
    AddCube(scene, "RightRail", Color32(255, 31, 92, 255),
            Vector3F(4.2F, 0.0F, -10.0F), Vector3F(0.12F, 2.4F, 14.0F));

    for (Int gate = 0; gate < 4; ++gate) {
        const Float z = -3.5F - gate * 4.0F;
        AddCube(scene, "GateLeft", Color32(25, 140, 255, 255),
                Vector3F(-3.4F, 2.0F, z), Vector3F(0.12F, 2.0F, 0.12F));
        AddCube(scene, "GateRight", Color32(255, 31, 92, 255),
                Vector3F(3.4F, 2.0F, z), Vector3F(0.12F, 2.0F, 0.12F));
        AddCube(scene, "GateTop", Color32(140, 64, 255, 255),
                Vector3F(0.0F, 4.0F, z), Vector3F(3.4F, 0.12F, 0.12F));
    }

    Node *spinner = AddCube(scene, "Spinner", Color32(38, 255, 204, 255),
                            Vector3F(0.0F, 0.4F, -5.0F), Vector3F(0.7F, 0.7F, 0.7F));
    spinner->SetName(String("Spinner"));
    scene->UpdateRS();
    return scene;
}

void UpdateEnvironment(Node *scene, Float elapsedTime) {
    if (!scene) return;
    Node *spinner = DynamicCast<Node>(scene->FindChild("Spinner"));
    if (!spinner) return;

    Matrix3F rotation(Vector3F::UNIT_Y, elapsedTime * 0.8F);
    spinner->Local.SetRotate(rotation);
}

}
