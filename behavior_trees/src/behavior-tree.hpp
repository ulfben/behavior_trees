#pragma once
#include "common.hpp"
#include "entity.hpp"
#include "world.hpp"

enum class Status{ Success, Failure, Running };

struct Context final{
    Entity& self;
    World& world;
    std::vector<std::string_view> debug_trace;
};

// Base node interface
struct Node{
    virtual ~Node() = default;
    virtual Status tick(Context& ctx, float dt) const noexcept = 0;
    virtual ~Node() = default; 
};

// Composite: Sequence
// Runs children left-to-right.
// IF a child fails, the sequence Fails immediately.
// IF a child runs, the sequence returns Running (and restarts from 0 next frame).
struct Sequence final : Node{
    std::vector<Node*> children;
    std::string_view name; //for debugging

    explicit Sequence(std::initializer_list<Node*> xs) : children(xs){}
    Sequence(std::initializer_list<Node*> xs, std::string_view _name) 
        : children(xs), name(_name){}

    Status tick(Context& ctx, float dt) const noexcept override{
        ctx.debug_trace.push_back(name);
        for(const auto* child : children){
            const Status s = child->tick(ctx, dt);
            if(s == Status::Running) return Status::Running;
            if(s == Status::Failure) return Status::Failure;
        }
        return Status::Success;
    }
};

// Composite: Selector
// Composite: Runs children left-to-right.
// IF a child succeeds, the selector Succeeds immediately.
// IF a child runs, the selector returns Running.
struct Selector final : Node{
    std::vector<Node*> children;
    std::string_view name; //for debugging

    explicit Selector(std::initializer_list<Node*> xs) : children(xs){}
    Selector(std::initializer_list<Node*> xs, std::string_view _name) 
        : children(xs), name(_name){}

    Status tick(Context& ctx, float dt) const noexcept override{ 
        ctx.debug_trace.push_back(name);
        for(const auto* child : children){
            const Status s = child->tick(ctx, dt);
            if(s == Status::Running) return Status::Running;
            if(s == Status::Success) return Status::Success;
        }
        return Status::Failure;
    }
};

// Composite: MemorySequence
// remembers progress inside a multi-step task, like walking between waypoints in order
struct MemorySequence final : Node{
    std::vector<Node*> children;
    int mem_slot = 0;
    std::string_view name; //for debugging

    MemorySequence(int slot, std::initializer_list<Node*> xs)
        : children(xs), mem_slot(slot){}
    MemorySequence(int slot, std::initializer_list<Node*> xs, std::string_view _name) 
        : children(xs), mem_slot(slot), name(_name){}

    Status tick(Context& ctx, float dt) const noexcept override{
        assert(mem_slot < ctx.self.bt_mem.size());
        ctx.debug_trace.push_back(name);
        int& i = ctx.self.bt_mem[mem_slot]; //grab a reference to the entity's memory of this behavior
        while(i < (int) children.size()){
            const Status s = children[i]->tick(ctx, dt);
            if(s == Status::Running){
                return Status::Running;
            }
            if(s == Status::Failure){ 
                i = 0; 
                return Status::Failure; 
            }
            ++i;
        }
        i = 0;
        return Status::Success;
    }   
};

//useful for "keep doing this unless something higher priority interrupts"
struct RepeatForever final : Node{
    Node* child{};
    std::string_view name; //for debugging        

    explicit RepeatForever(Node* c) : child(c){}
    RepeatForever(Node* c, std::string_view _name) : child(c), name(_name){}

    Status tick(Context& ctx, float dt) const noexcept override{
        ctx.debug_trace.push_back(name);
        std::ignore = child->tick(ctx, dt);
        return Status::Running;
    }
};

// Leaf node: either a condition or an action, supplied as a function pointer.
// Could use std::function or lambdas, but we opt for plain function pointer 
// to enforce that leaf nodes are stateless; all behavior state lives in Context
// The leaf nodes are the ones what actually interacts with the world
using LeafFn = Status(*)(Context&, float) noexcept;

struct Leaf final : Node{
    LeafFn fn{};
    std::string_view name; //for debugging     

    explicit Leaf(LeafFn f) : fn(f){}
    Leaf(LeafFn f, std::string_view _name) 
        : fn(f), name(_name){}

    Status tick(Context& ctx, float dt) const noexcept override{ 
        ctx.debug_trace.push_back(name);
        return fn(ctx, dt); 
    }
};

struct EntityBrain final{
    Node* root = nullptr;
    Status tick(Context& ctx, float dt) const noexcept{
        assert(root);        
        return root->tick(ctx, dt);
    }
};