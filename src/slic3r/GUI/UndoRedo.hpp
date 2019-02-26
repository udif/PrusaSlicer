#ifndef UNDOREDO_HPP
#define UNDOREDO_HPP

#include <libslic3r/Model.hpp>

#include <memory>

namespace Slic3r {


class UndoRedo {
public:
    UndoRedo(Model* model) : m_model(model) {}

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

    // ModelObject - deletion / name change / new instance / new ModelVolume / config change / layer_editing
    // ModelInstance - deletion / transformation matrix change
    // ModelVolume - deletion / transformation change / ModelType change / name change / config change

    void begin_batch(const std::string& desc);
    void end_batch();

    bool anything_to_redo() const {
        return (!m_stack.empty() && m_index != m_stack.size());
    }

    bool anything_to_undo() const {
        return (!m_stack.empty() && m_index != 0);
    }

    void undo();
    void redo();

    std::string get_undo_description() const {
        return m_stack[m_index-1]->m_description;
    }
    std::string get_redo_description() const {
        return m_stack[m_index]->m_description;
    }



private:
    class Command {
    public:
        virtual void redo() = 0;
        virtual void undo() = 0;
        virtual ~Command() {}
        bool m_bound_to_previous = false;
        std::string m_description;
        Model* m_model;
    };

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

    /*class AddInstance : public Command {
    public:
        AddInstance(int mo_idx, int mi_idx);
        void redo() override;
        void undo() override;
    private:
        int m_mo_idx;
        int m_mi_idx;
    };

    class RemoveInstance : public Command {
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

private:
    void action(Command* command);
    void push(Command* command);
    bool working() const;

private:
	std::vector<std::unique_ptr<Command>> m_stack;
	unsigned int m_index = 0;
    std::string m_batch_desc = "";
    bool m_batch_start = false;
    bool m_batch_running = false;
    Model* m_model;
};


} // namespace Slic3r

#endif // UNDOREDO_HPP
