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
    std::string_view description{}; //for debug traces

    explicit Node(std::string_view n = {}) : description(n){}
    virtual ~Node() = default;

    Status tick(Context& ctx, float dt) const noexcept{
        ctx.debug_trace.push_back(description);
        return do_tick(ctx, dt);
    }

private:
    virtual Status do_tick(Context& ctx, float dt) const noexcept = 0;
};

// Composite: Sequence
// Runs children left-to-right.
// IF a child fails, the sequence Fails immediately.
// IF a child runs, the sequence returns Running (and restarts from 0 next frame).
struct Sequence final : Node{
    std::vector<Node*> children;

    explicit Sequence(std::initializer_list<Node*> xs) : children(xs){}
    Sequence(std::initializer_list<Node*> xs, std::string_view _name) 
        : Node(_name), children(xs){}

private:
    Status do_tick(Context& ctx, float dt) const noexcept override{        
        for(const auto* child : children){
            const Status s = child->tick(ctx, dt);
            if(s == Status::Running){                
                return Status::Running;
            }
            if(s == Status::Failure){
                ctx.debug_trace.push_back("\t\t\tNo"sv);
                return Status::Failure;
            }
            ctx.debug_trace.push_back("\t\t\tYes"sv);
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
    Selector(std::initializer_list<Node*> xs, std::string_view _name) 
        : Node(_name), children(xs){}

private:
    Status do_tick(Context& ctx, float dt) const noexcept override{        
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

    MemorySequence(int slot, std::initializer_list<Node*> xs)
        : children(xs), mem_slot(slot){}
    MemorySequence(int slot, std::initializer_list<Node*> xs, std::string_view _name) 
        : Node(_name), children(xs), mem_slot(slot){}

private:
    Status do_tick(Context& ctx, float dt) const noexcept override{
        assert(mem_slot < ctx.self.bt_mem.size());   
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

    explicit RepeatForever(Node* c) : child(c){}
    RepeatForever(Node* c, std::string_view _name) : Node(_name), child(c){}

private:
    Status do_tick(Context& ctx, float dt) const noexcept override{        
        std::ignore = child->tick(ctx, dt);
        return Status::Running;
    }
};

// Leaf node: either a condition or an action, supplied as a function pointer.
// Could use std::function or lambdas, but we opt for plain function pointer 
// to enforce that leaf nodes are stateless; all behavior state lives in Context
// The leaf nodes are the ones that actually interacts with the world
using LeafFn = Status(*)(Context&, float) noexcept;

struct Leaf final : Node{
    LeafFn fn{};    

    explicit Leaf(LeafFn f) : fn(f){}
    Leaf(LeafFn f, std::string_view _name) 
        : Node(_name), fn(f){}

private:
    Status do_tick(Context& ctx, float dt) const noexcept override{        
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