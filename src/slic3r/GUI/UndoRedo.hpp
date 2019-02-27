#ifndef UNDOREDO_HPP
#define UNDOREDO_HPP

#include <libslic3r/Model.hpp>

#include <memory>

namespace Slic3r {


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

    // ModelObject - deletion / name change / new instance / new ModelVolume / config change / layer_editing
    // ModelInstance - deletion / transformation matrix change
    // ModelVolume - deletion / transformation change / ModelType change / name change / config change

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

    void action(Command* command);
    void push(Command* command);
    void print_stack() const;

	std::vector<std::unique_ptr<Command>> m_stack;
	unsigned int m_index = 0;
    std::string m_batch_desc = "";
    unsigned int m_batch_depth = 0; // how many times begin_batch was called (to allow nesting)
    bool m_batch_running = false;
    Model* m_model;
};


} // namespace Slic3r

#endif // UNDOREDO_HPP
