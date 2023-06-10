# Component Fuse Mechanic

## Overview

This repo features an actor component that adds a "fuse" mechanic to the player, inspired by the _Ultrahand_ mechanic in _The Legend of Zelda: Tears of the Kingdom_. This is not a production-ready system, and was developed as more of a programming exercise and experiment with Chaos.

The project is developed in **Unreal Engine 5.2**, meaning that is the earliest version it supports. This decision was made due to some stability improvements for Chaos physics introduced in 5.2, but the code should work fine in earlier versions.

The following mechanics are included:

* Pick up and translate/rotate objects in the world by rounded values, relative to the player.
* Draw orthographic projections of grabbed fusible objects (uses 3 scene captures + render targets, not ideal)
* Search through sockets on meshes that contain "Attach" in the name, and find the closest sockets between two objects as well as supplementary sockets that would be very close.
* Detect potential clipping issues and prevent constraints that would cause them.
* Snap objects together at the socket and constrain them at the closest sockets, and the supplementary sockets, with a physics-based interp.
* Detach objects from other "fused" objects (rudimentary)

| Keybind             | Action                                                  |
| ------------------- | ------------------------------------------------------- |
| Right Mouse Button  | Enter searching mode                                    |
| F                   | Pick up targeted fusible/try fuse grabbed fusible       |
| Mouse Wheel         | Adjust grabbed fusible target XY distance               |
| Arrow keys          | Rotate grabbed fusible                                  |
| Q                   | Break constraints on grabbed fusible                    |
| R                   | Interact with interactable objects.                     |


| Asset               | Source                            |
| ------------------- | --------------------------------- |
| Props               | Evelyn Schwab                     |
| Character           | Epic Games, third person template |
| Movement anims      | Caleb Longmire, ALS               |

Use _**f.showdebugfuser 1**_ to draw debug information (including detected fuse operations)
