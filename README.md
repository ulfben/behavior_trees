# Behavior Trees Demo
Demo code for my workshop on Behavior Trees in games.

## Workshop Brief: Reverse-Engineering a Behavior Tree

In this workshop you will work with a small but complete AI system implemented using a Behavior Tree.
You are not expected to understand the implementation in advance.

Before the workshop, you are expected to have read the assigned chapter on Behavior Trees in the course literature.

AI For Games, 3rd Edition, Chapter 5


Your task during the workshop is to observe, trace, and reason about how the AI actually behaves, based on running code - not diagrams or theory.

The goal is to build a correct mental model of how this AI works over time, and to compare that model to how Behavior Trees are commonly described in the literature.

## Source Structure
* **`common.hpp`**
    Foundational configuration, math utilities, and Raylib helpers used across the project.
    
* **`window.hpp`**
    An RAII wrapper for Raylib that manages the window lifecycle

* **`entity.hpp`**
    Defines the agent data model, including physics state (position, velocity), rendering, and individual AI memory.

* **`world.hpp`**
    Manages global environmental state, such as waypoints, hazards (the Wolf), and resources (Food).

* **`steering.hpp`**
    Stateless physics helpers that calculate steering forces (such as; seek, flee) to drive entity movement.

* **`behavior-tree.hpp`**
    The generic AI engine. Defines the core architecture: `Node` interface, Composites (`Selector`, `Sequence`), and the execution `Context`.

* **`game-ai.hpp`**
    The game-specific logic. Implements the concrete Leaf nodes (conditions/actions) and assembles the specific Behavior Tree used in the demo.

* **`main.cpp`**
    The application entry point. Initializes the systems and executes the primary game loop.

---

## Tasks

### 1. Run and trace the demo

1.  Clone the repository, build, and run the application.
2.  Run the program in a debugger.
3.  Step through execution frame-by-frame and follow how `tick()` propagates through the tree.

**Observe:**
* Which nodes are evaluated every frame?
* Which nodes return `Running`, `Success`, or `Failure`?
* Which branches are skipped as a result?

*You are encouraged to add temporary debug output (logging, breakpoints, overlays) to help you understand what is happening.*

### 2. Create an execution diagram

Create a visual diagram that shows how the AI behaves over time. Your diagram should include:

* The order in which nodes are ticked.
* Where and why execution stops each frame.
* Which node is “in control” during each behavior.

*The diagram does not need to match textbook diagrams. Any clear format is acceptable (hand-drawn, digital, ASCII, etc.).*

### 3. Compare implementation to the literature

Identify and describe at least two ways in which this implementation differs from how Behavior Trees are presented in the literature. Examples might include:

* How often nodes are re-evaluated.
* How long-running actions are represented.
* Where and how state is stored.

*Focus on behavior and execution, not naming or API differences.*

### 4. Add a new behavior: free wandering after patrol

Extend the AI so that:

1.  After visiting all four waypoints, the agent stops patrolling.
2.  Instead, it wanders freely around the map.

**The new behavior must:**
* Integrate into the existing Behavior Tree structure.
* Not break existing behaviors (fleeing, hunger, etc.).

*There is no single correct solution—justify your design choices.*

### 5. Add a new behavior: reversed patrol when caught

Extend the AI so that:

1.  If the agent is caught by the wolf, it continues patrolling.
2.  But visits the waypoints in **reverse order**.

**You must decide:**
* Where this state lives.
* How it affects existing behavior.

### 6. Add a new behavior: allow the AI to randomly choose a behavior

When the agent is not under attack, it should select either Stand Still, Wander or Patrol at random.
Once a behavior has been selected, it must be allowed to run for some time. The AI should not oscillate between behaviors on consecutive ticks due to repeated random selection. 

*Avoid introducing delays, sleeps, or timers—all behavior should still be driven by the Behavior Tree tick.*