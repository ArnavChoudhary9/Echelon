#pragma once

// --- Core ---
#include "Core/Base.hpp"
#include "Core/KeyCodes.hpp"
#include "Core/MouseCodes.hpp"
#include "Core/Clock.hpp"

#include "Application/Application.hpp"
#include "Instrumentation/Instrumentation.hpp"

// --- Logger ---
#include "Logger/Logger.hpp"
#include "Logger/Sink.hpp"
#include "Core/Log.hpp"

// --- Events ---
#include "Events/Event.hpp"
#include "Events/ApplicationEvent.hpp"
#include "Events/KeyEvents.hpp"
#include "Events/MouseEvent.hpp"

// --- Layer ---
#include "Layer/Layer.hpp"
#include "Layer/Overlay.hpp"
#include "Layer/LayerStack.hpp"

// --- ECS, Scene and Projects ---
#include "ECS/Entity.hpp"
#include "ECS/Components.hpp"
#include "Scene/Scene.hpp"
#include "Scene/SceneSerializer.hpp"
#include "Scene/SceneGraph.hpp"
#include "Project/Project.hpp"

// --- Renderer Plugin ---
#include "Renderer/Renderer.hpp"

// --- Graphics Abstraction Layer ---
#include "GraphicsAPI/GraphicsIncludes.hpp"

// --- Platform Abstraction Layer ---
#include "Platform/PlatformIncludes.hpp"
