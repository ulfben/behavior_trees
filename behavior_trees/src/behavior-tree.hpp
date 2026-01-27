#pragma once
#include "common.hpp"
#include "entity.hpp"
#include "world.hpp"

enum class Status{ Success, Failure, Running };

struct Context final{
    Entity& self;
    World& world;
};

// Base node interface
struct Node{
    virtual ~Node() = default;
    virtual Status tick(Context& ctx, float dt) const noexcept = 0;
};

// Composite: Sequence
// Runs children left-to-right.
// IF a child fails, the sequence Fails immediately.
// IF a child runs, the sequence returns Running (and restarts from 0 next frame).
struct Sequence final : Node{
    std::vector<Node*> children;
    explicit Sequence(std::initializer_list<Node*> xs) : children(xs){}

    Status tick(Context& ctx, float dt) const noexcept override{
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
    explicit Selector(std::initializer_list<Node*> xs) : children(xs){}

    Status tick(Context& ctx, float dt) const noexcept override{
        for(const auto* child : children){
            const Status s = child->tick(ctx, dt);
            if(s == Status::Running) return Status::Running;
            if(s == Status::Success) return Status::Success;
        }
        return Status::Failure;
    }
};

// Composite: MemorySequence
// remembers which child was running for this entity.
struct MemorySequence final : Node{
    std::vector<Node*> children;
    int mem_slot = 0;

    MemorySequence(int slot, std::initializer_list<Node*> xs)
        : children(xs), mem_slot(slot){}

    Status tick(Context& ctx, float dt) const noexcept override{
        int& i = ctx.self.bt_mem[mem_slot];
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

struct RepeatForever final : Node{
    Node* child{};
    explicit RepeatForever(Node* c) : child(c){}

    Status tick(Context& ctx, float dt) const noexcept override{
        std::ignore = child->tick(ctx, dt);
        return Status::Running;
    }
};

// Leaf node: either a condition or an action, supplied as a function pointer.
// Could use std::function or lambdas, but we opt for plain function pointer 
// to enforce that leaf nodes are stateless; all behavior state lives in Context
using LeafFn = Status(*)(Context&, float) noexcept;

struct Leaf final : Node{
    LeafFn fn{};
    explicit Leaf(LeafFn f) : fn(f){}
    Status tick(Context& ctx, float dt) const noexcept override{ return fn(ctx, dt); }
};

struct EntityBrain final{
    Node* root = nullptr;
    Status tick(Context& ctx, float dt) const noexcept{
        assert(root);
        return root->tick(ctx, dt);
    }
};