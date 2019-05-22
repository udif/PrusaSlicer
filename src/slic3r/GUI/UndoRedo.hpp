//----------------------------------
// ARCHITECTURE DESCRIPTION
//
// - UndoRedo object is a part of Model - each Model has its own with the whole history
// Undoable actions:
    // ModelObject - deletion / name change / new instance / new ModelVolume / config change / layer_editing
    // ModelInstance - deletion / transformation matrix change
    // ModelVolume - deletion / transformation change / ModelType change / name change / config change
    
    // NOT undoable actions (not part of Model)
    // global config changes / switching profiles    


// - Model is only modified through UndoRedo public methods - ideally, no one else has write access

// - when one of these methods is called, an object derived from abstract Command class is created and everything
//   that needs to be done (which object is being manipulated and how, etc.) is saved as private data in this object

// - Objects, instances and volumes are referencd by the ids in the respective vector. This ensures that same objects
//   are referenced no matter how the stack is traversed (i.e., no pointers are saved)

// - UndoRedo manages the stack (std::vector<std::unique_ptr<Command*>>) and keeps an index to current position

// - the unique_ptr to this object is then pushed on the stack and virtual redo() method is called to 
//   do the action (this ensures that doing and redoing the action goes through the same function and there is no need
//   to have the code in two places and keep it in sync)

// - if several actions are supposed to be undone simultaneously (e.g. arrange = multiple consecutive object manipulation),
//   the Command is flagged to be bound to previous Command. When undo is invoked, actions are being undone one by one,
//   until one not bound to the previous is encountered. Whenever one wants to stack actions like this, it is enough
//   to use RAII helper (as seen in ModelArrange.cpp (632)

// - moving back the stack and then doing any action clears all the stack records past the current index position.
//   This allows all resources (saved meshes, etc) to be released in RAII manner in Command child (virtual) destructor.


//----------------------------------
// PROs: - it should be relatively simple to extend current code without major modifications
//       - managing the actions and data needed to do them is encapsulated in the Command-derived objects
//       - only changes are saved, not the whole scene state
//       - UndoRedo does not have to be aware of actions that can be divided into several simpler ones (such as arrange or cut)

// CONs: - some actions are saved needlessly (such as center_around_origin, etc.)


//-----------------------------------
// Issues to solve
// * accessing and managing the UndoRedo object is currently too verbose. the global Model UndoRedo should be accessible
//   (auto undo_scoped_batch = m_objects->front()->get_model()->undo->begin_scoped_batch("Delete subobject");)
//
// * moving objects from one Model to another (which happens when loading objects, 3MFs, etc.)
//      - the idea is to take all actions that modify the moved object from one stack and push them as a batch to the other
//        (adding that object would then appear as a single action from the outside world)

// * take care of proper synchronization between 3D scene and ObjectList

// * make sure that any action stops background processing (all actions invoke the UndoRedo::action(...) function, so it should
//   be enough to do it there) and invalidate what necessary (apply()) - I don't think it is worth taking care of revalidating
//   objects when an action is undone and then redone again.

// * decide about where to store meshes of deleted volumes. It could be possible to keep them in memory until this action is pushed
//   deeper into the stack, them dump it to temp file (which could be done in separate thread) to release the memory.



//------------------------------------
// CURRENT STATUS:
// - not meant as candidate to merge, just a curious novice attempt
// - has not been rebased onto master for quite some time
// - transformation (incl. arrange and place to bed), changing name and type of object can be undone/redone
// - adding/removing instances partly works
// - adding/removing volumes is not finished
// - often crashes (doing an action that is not fully covered in UndoRedo makes UndoRedo find inconsistencies, 
//   it attempts to undo transformation on object that was deleted by its back, etc.) This problem should
//   solve itself when all actions are done through the UndoRedo pipeline





#ifndef UNDOREDO_HPP
#define UNDOREDO_HPP

#include <libslic3r/Model.hpp>

#include <memory>

namespace Slic3r {

    
static int get_idx(ModelInstance* inst)
{
    const ModelObject* mo = inst->get_object();
    int i=0;
    for (i=0; i<(int)mo->instances.size(); ++i)
        if (mo->instances[i] == inst)
            return i;
    return -1;
}

static int get_idx(ModelObject* mo)
{
    const Model* model = mo->get_model();
    int i=0;
    for (i=0; i<(int)model->objects.size(); ++i)
        if (model->objects[i] == mo)
            return i;
    return -1;
}


class UndoRedo {
public:
    UndoRedo(Model* model) : m_model(model) {}

    // Calling either of following functions creates an instance of the Command object,
    // pushes in onto the stack and calls its redo() function to actually modify the Model
    void change_instance_transformation(ModelInstance* inst, const Geometry::Transformation& t) {
        action(new ChangeInstanceTransformation(m_model, inst, t));
    }
    void change_instance_transformation(int mo_idx, int mi_idx, const Geometry::Transformation& t) {
        action(new ChangeInstanceTransformation(m_model, mo_idx, mi_idx, t));
    }
    void change_volume_transformation(ModelVolume* vol, const Geometry::Transformation& t) {
        action(new ChangeVolumeTransformation(m_model, vol, t));
    }
    void change_volume_transformation(int mo_idx, int mv_idx, const Geometry::Transformation& t) {
        action(new ChangeVolumeTransformation(m_model, mo_idx, mv_idx, t));
    }
    void change_name(int mo_idx, int mv_idx, const std::string& name) {
        action(new ChangeName(m_model, mo_idx, mv_idx, name));
    }
    void change_volume_type(ModelVolume* vol, ModelVolumeType vol_type) {
        action(new ChangeVolumeType(m_model, vol, vol_type));
    }
    void change_volume_type(int mo_idx, int mv_idx, ModelVolumeType vol_type) {
        action(new ChangeVolumeType(m_model, mo_idx, mv_idx, vol_type));
    }
    void add_instance(ModelObject* mo, int mi_idx = -1, const Geometry::Transformation& t = Geometry::Transformation()) {
        action(new AddInstance(m_model, mo, mi_idx, t));
    }
    void remove_instance(ModelObject* mo, int mi_idx = -1) {
        action(new RemoveInstance(m_model, mo, mi_idx));
    }
    void add_volume(ModelObject* mo, int mv_idx, TriangleMesh mesh) {
        action(new AddVolume(m_model, get_idx(mo), mv_idx, mesh));
    }


    // Wrapping the actions between begin_batch() and end_batch() means they will
    // be marked as connected together to be undone/redone together.
    void begin_batch(const std::string& desc);
    void end_batch();

    // RAII helper object to ensure pairing of begin_batch - end_batch.
    class ScopedBatch
    {
    public:
        ScopedBatch(UndoRedo* undo, const std::string& desc) : m_undo(undo) { undo->begin_batch(desc); }
        ~ScopedBatch() { m_undo->end_batch(); }

    private:
        UndoRedo* m_undo; // backpointer to parent object
    };
    // Calls begin_batch and returns an object which automatically calls end_batch on destruction (RAII).
    ScopedBatch begin_scoped_batch(const std::string& desc) { return ScopedBatch(this, desc); };

    // Returns information about current stack index position.
    bool anything_to_redo() const { return (!m_stack.empty() && m_index != m_stack.size()); }
    bool anything_to_undo() const { return (!m_stack.empty() && m_index != 0); }
    std::string get_undo_description() const { return m_stack[m_index-1]->m_description; }
    std::string get_redo_description() const { return m_stack[m_index]->m_description;   }

    // Performs the undo/redo and moves the stack index accordingly.
    void undo();
    void redo();

private:
    // Abstract class representing an undoable/redoable action.
    class Command {
    public:
        virtual void redo() = 0;
        virtual void undo() = 0;
        virtual ~Command() {}
        bool m_bound_to_previous = false;
        std::string m_description;
        Model* m_model;
    };

    // Following classes represent concrete undoable actions. Any information necessary to perform the action
    // is saved as a member variable in constructor.
    class ChangeInstanceTransformation : public Command {
    public:
        ChangeInstanceTransformation(Model* model, int mo_idx, int mi_idx, const Geometry::Transformation& transformation);
        ChangeInstanceTransformation(Model* model, ModelInstance* instance, const Geometry::Transformation& transformation);
        void redo() override;
        void undo() override;
    private:
        int m_mo_idx;
        int m_mi_idx;
        Geometry::Transformation m_old_transformation;
        Geometry::Transformation m_new_transformation;
    };

    class AddInstance : public Command {
    public:
        AddInstance(Model* model, ModelObject* mo, int mi_idx, const Geometry::Transformation& t);
        AddInstance(Model* model, int mo_idx, int mi_idx, const Geometry::Transformation& t);
        void redo() override;
        void undo() override;
    private:
        int m_mo_idx;
        int m_mi_idx;
        Geometry::Transformation m_transformation;
    };

    class RemoveInstance : public Command {
    public:
        RemoveInstance(Model* model, ModelObject* mo, int mi_idx);
        RemoveInstance(Model* model, int mo_idx, int mi_idx);
        void redo() override;
        void undo() override;
    private:
        AddInstance m_command_add_instance;
    };

    /*class RemoveInstance : public Command {
    public:
        RemoveInstance(int mo_idx, int mi_idx);
        void redo() override;
        void undo() override;
    private:
        int m_mo_idx;
        int m_mi_idx;
    };*/

    class ChangeName : public Command {
    public:
        ChangeName(Model* m_model, int mo_idx, int mv_idx, const std::string& name);
        void redo() override;
        void undo() override;
    private:
        int m_mo_idx;
        int m_mv_idx;
        std::string m_old_name;
        std::string m_new_name;
    };

    class ChangeVolumeType : public Command {
    public:
        ChangeVolumeType(Model* model, int mo_idx, int mv_idx, ModelVolumeType new_type);
        ChangeVolumeType(Model* model, ModelVolume* vol, ModelVolumeType new_type);
        void redo() override;
        void undo() override;
    private:
        int m_mo_idx;
        int m_mv_idx;
        ModelVolumeType m_old_type;
        ModelVolumeType m_new_type;
    };

    class ChangeVolumeTransformation : public Command {
    public:
        ChangeVolumeTransformation(Model* model, ModelVolume* vol, const Geometry::Transformation& transformation);
        ChangeVolumeTransformation(Model* model, int mo_idx, int mv_idx, const Geometry::Transformation& transformation);
        void redo() override;
        void undo() override;
    private:
        int m_mo_idx;
        int m_mv_idx;
        Geometry::Transformation m_old_transformation;
        Geometry::Transformation m_new_transformation;
    };
    
    class AddVolume : public Command {
    public:
        AddVolume(Model* model, int mo_idx, int mv_idx, TriangleMesh mesh);
        void redo() override;
        void undo() override;
    private:
        int m_mo_idx;
        int m_mv_idx;
        TriangleMesh m_mesh;
    };


// These are the private methods and variables that manage the stack itself:
    void action(Command* command); // push to stack and perform (all undoable actions go through this)
    void push(Command* command);   // pushes to stack
    void print_stack() const;      // prints stack (incl. current index position) to stdout

	std::vector<std::unique_ptr<Command>> m_stack;
	unsigned int m_index = 0;
    std::string m_batch_desc = "";
    unsigned int m_batch_depth = 0; // how many times begin_batch was called (to allow nesting)
    bool m_batch_running = false;
    Model* m_model;
};


} // namespace Slic3r

#endif // UNDOREDO_HPP
