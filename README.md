# 🌳 Tree of Legends

**Tree of Legends** is a game project developed using Unreal Engine 5, focused on interactive trees, cutting mechanics, resource gathering, and physics-based gameplay.

## 🎮 About the Project

The goal is to create a world where players can explore, cut down different types of trees, collect materials, and interact with the environment.

The project is currently under active development.

## 🪓 Core Gameplay Systems

### Tree Cutting System
- Procedural tree cutting.
- Different tree materials, including Wood and Iron.
- Axe strength and cutting resistance.
- Individual cutting progress for each tree.
- Physics-based tree fragments.

### Tree Activation System
- Trees initially use lightweight Static Mesh representations.
- A successful axe hit activates the procedural cutting system.
- Only the affected tree becomes active.

### Data-Driven Trees
- `DT_Tree` — tree configuration Data Table.
- Support planned for multiple tree types.
- Shared cutting logic using `BP_Tree_Procedural` and `TOLCutMeshData`.

## 🛠️ Technologies

- Unreal Engine 5
- Blueprint Visual Scripting
- C++
- Procedural Mesh Component
- Blender
- Git and GitHub

## 🚧 Development Status

**Work in Progress — Prototype**

Current development focuses on improving procedural cutting, branch geometry, tree activation, and stability.

Some features are experimental and may change.

## 📌 Planned Features

- More tree species and materials.
- Improved branch cutting.
- Resource collection.
- Physics interactions.
- Building and placement systems.
- Multiplayer support.

## 📄 License

License information will be added later.
