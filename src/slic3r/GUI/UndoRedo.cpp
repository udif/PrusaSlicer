#include "UndoRedo.hpp"
#include "GUI_App.hpp"
#include "GUI_ObjectList.hpp"
#include "GLCanvas3D.hpp"

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

static int get_idx(ModelVolume* vol)
{
    const ModelObject* mo = vol->get_object();
    int i=0;
    for (i=0; i<(int)mo->volumes.size(); ++i)
        if (mo->volumes[i] == vol)
            return i;
    return -1;
}

static void reload_object_list()
{
    GUI::wxGetApp().obj_list()->delete_all_objects_from_list();
    for (size_t idx=0; idx<GUI::wxGetApp().model_objects()->size(); ++ idx)
        GUI::wxGetApp().obj_list()->add_object_to_list(idx);
}

////////////////////////////////////////////////////////////////

void UndoRedo::action(Command* command)
{
    command->redo();  // don't forget to actually perform the action
    push(command);    // and push the command onto the undo/redo stack
}

////////////////////////////////////////////////////////////////

UndoRedo::ChangeInstanceTransformation::ChangeInstanceTransformation(Model* model, ModelInstance* inst, const Geometry::Transformation& transformation)
: ChangeInstanceTransformation(model, get_idx(inst->get_object()), get_idx(inst), transformation) {}

UndoRedo::ChangeInstanceTransformation::ChangeInstanceTransformation(Model* model, int mo_idx, int mi_idx, const Geometry::Transformation& transformation)
: m_mo_idx(mo_idx), m_mi_idx(mi_idx), m_new_transformation(transformation)
{
    m_model = model;
    m_description = "Change instance transformation";
    try {
    m_old_transformation = m_model->objects.at(m_mo_idx)->instances.at(mi_idx)->get_transformation();
    } catch (...) { std::cout << "!!!" << std::endl; m_old_transformation = Geometry::Transformation(); }
}

void UndoRedo::ChangeInstanceTransformation::undo()
{
    m_model->objects[m_mo_idx]->instances[m_mi_idx]->m_transformation = m_old_transformation;
}

void UndoRedo::ChangeInstanceTransformation::redo()
{
    m_model->objects[m_mo_idx]->instances[m_mi_idx]->m_transformation = m_new_transformation;
}

////////////////////////////////////////////////////////////////

UndoRedo::ChangeVolumeTransformation::ChangeVolumeTransformation(Model* model, ModelVolume* vol, const Geometry::Transformation& transformation)
: ChangeVolumeTransformation(model, get_idx(vol->get_object()), get_idx(vol), transformation) {}

UndoRedo::ChangeVolumeTransformation::ChangeVolumeTransformation(Model* model, int mo_idx, int mv_idx, const Geometry::Transformation& transformation)
: m_mo_idx(mo_idx), m_mv_idx(mv_idx), m_new_transformation(transformation)
{
    m_model = model;
    m_description = "Change volume transformation";
    m_old_transformation = m_model->objects[m_mo_idx]->volumes[mv_idx]->get_transformation();
}

void UndoRedo::ChangeVolumeTransformation::undo()
{
    m_model->objects[m_mo_idx]->volumes[m_mv_idx]->m_transformation = m_old_transformation;
}

void UndoRedo::ChangeVolumeTransformation::redo()
{
    m_model->objects[m_mo_idx]->volumes[m_mv_idx]->m_transformation = m_new_transformation;
}

////////////////////////////////////////////////////////////////

UndoRedo::ChangeVolumeType::ChangeVolumeType(Model* model, ModelVolume* vol, ModelVolume::Type new_type)
: ChangeVolumeType(model, get_idx(vol->get_object()), get_idx(vol), new_type) {}

UndoRedo::ChangeVolumeType::ChangeVolumeType(Model* model, int mo_idx, int mv_idx, ModelVolume::Type new_type)
: m_mo_idx(mo_idx), m_mv_idx(mv_idx), m_new_type(new_type)
{
    m_model = model;
    m_description = "Change volume type";
    m_old_type = m_model->objects[m_mo_idx]->volumes[mv_idx]->type();
}

void UndoRedo::ChangeVolumeType::undo()
{
    m_model->objects[m_mo_idx]->volumes[m_mv_idx]->m_type = m_old_type;
}

void UndoRedo::ChangeVolumeType::redo()
{
    m_model->objects[m_mo_idx]->volumes[m_mv_idx]->m_type = m_new_type;
}

////////////////////////////////////////////////////////////////

UndoRedo::ChangeName::ChangeName(Model* model, int mo_idx, int mv_idx, const std::string& new_name)
: m_mo_idx(mo_idx), m_mv_idx(mv_idx), m_new_name(new_name)
{
    m_model = model;
    m_description = "Change volume name";
    m_old_name = mv_idx != -1 ? m_model->objects[m_mo_idx]->volumes[mv_idx]->name :  m_model->objects[m_mo_idx]->name;
}

void UndoRedo::ChangeName::undo()
{
    if (m_mv_idx >= 0)
        m_model->objects[m_mo_idx]->volumes[m_mv_idx]->name = m_old_name;
    else
        m_model->objects[m_mo_idx]->name = m_old_name;

    reload_object_list();
}

void UndoRedo::ChangeName::redo()
{
    if (m_mv_idx >= 0)
        m_model->objects[m_mo_idx]->volumes[m_mv_idx]->name = m_new_name;
    else
        m_model->objects[m_mo_idx]->name = m_new_name;

    reload_object_list();
}

////////////////////////////////////////////////////////////////



/*
void UndoRedo::Change::redo()
{
    if (m_mi_idx != -1)
        m_model->objects[m_mo_idx]->instances[m_mi_idx]->set_transformation(m_new_trans);
    else {
        m_model->objects[m_mo_idx]->volumes[m_mv_idx]->set_transformation(m_new_trans);
        m_model->objects[m_mo_idx]->volumes[m_mv_idx]->name = m_new_name;
        m_model->objects[m_mo_idx]->volumes[m_mv_idx]->set_type(m_new_type);
    }

    GUI::wxGetApp().obj_list()->delete_all_objects_from_list();
    for (size_t idx=0; idx<m_model->objects.size(); ++ idx)
        GUI::wxGetApp().obj_list()->add_object_to_list(idx);
}

void UndoRedo::Change::undo()
{
    if (m_mi_idx != -1)
        m_model->objects[m_mo_idx]->instances[m_mi_idx]->set_transformation(m_old_trans);
    else{
        m_model->objects[m_mo_idx]->volumes[m_mv_idx]->set_transformation(m_old_trans);
        m_model->objects[m_mo_idx]->volumes[m_mv_idx]->name = m_old_name;
        m_model->objects[m_mo_idx]->volumes[m_mv_idx]->set_type(m_old_type);
    }

    GUI::wxGetApp().obj_list()->delete_all_objects_from_list();
    for (size_t idx=0; idx<m_model->objects.size(); ++ idx)
        GUI::wxGetApp().obj_list()->add_object_to_list(idx);
}

////////////////////////////////////////////////////////////////

UndoRedo::AddInstance::AddInstance(ModelInstance* mi, unsigned int mo_idx) : m_mo_idx(mo_idx)
{
    m_model = mi->get_object()->get_model();
    m_trans = mi->get_transformation();
}

void UndoRedo::AddInstance::redo()
{
    m_model->objects[m_mo_idx]->add_instance()->set_transformation(m_trans);
    GUI::wxGetApp().obj_list()->increase_object_instances(m_mo_idx, 1);
}

void UndoRedo::AddInstance::undo()
{
    m_model->objects[m_mo_idx]->delete_last_instance();
    GUI::wxGetApp().obj_list()->decrease_object_instances(m_mo_idx, 1);

}

////////////////////////////////////////////////////////////////

UndoRedo::RemoveInstance::RemoveInstance(Model* model, Geometry::Transformation trans, unsigned int mo_idx, unsigned int mi_idx)
: m_mo_idx(mo_idx), m_mi_idx(mi_idx), m_trans(trans)
{
    m_model = model;
}

void UndoRedo::RemoveInstance::redo()
{
    m_model->objects[m_mo_idx]->delete_instance(m_mi_idx);
    GUI::wxGetApp().obj_list()->decrease_object_instances(m_mo_idx, 1);
}

void UndoRedo::RemoveInstance::undo()
{
    m_model->objects[m_mo_idx]->add_instance(m_mi_idx)->set_transformation(m_trans);
    GUI::wxGetApp().obj_list()->increase_object_instances(m_mo_idx, 1);
}
*/
////////////////////////////////////////////////////////////////

void UndoRedo::begin_batch(const std::string& desc)
{
    if (m_batch_start || m_batch_running)
        throw std::runtime_error("UndoRedo does not allow nested batches.");
    m_batch_desc = desc;
    m_batch_start = true;
}


void UndoRedo::end_batch() {
    if (!m_batch_start && !m_batch_running)
        throw std::runtime_error("UndoRedo batch closed when none was started.");

    m_batch_start = false;
    m_batch_running = false;
}
/*
void UndoRedo::begin(ModelInstance* inst)
{
    if (working())
        return;

    if (m_current_command_type != CommandType::None)
        throw std::runtime_error("Undo/Redo stack - attemted to nest commands.");

    ModelObject* mo = inst->get_object();
    m_model_instance_data.mi_idx = get_idx(inst);
    m_model_instance_data.inst_num = mo->instances.size();
    m_model_instance_data.mo_idx = get_idx(mo);
    m_model_instance_data.transformation = inst->get_transformation();
    m_current_command_type = CommandType::ModelInstanceManipulation;
}

void UndoRedo::begin(ModelVolume* vol)
{
    if (working())
        return;

    if (m_current_command_type != CommandType::None)
        throw std::runtime_error("Undo/Redo stack - attemted to nest commands.");

    m_model_volume_data.mv_idx = get_idx(vol);
    m_model_volume_data.mo_idx = get_idx(vol->get_object());
    m_model_volume_data.vols_num = vol->get_object()->volumes.size();
    m_model_volume_data.transformation = vol->get_transformation();
    m_model_volume_data.name = vol->name;
    m_model_volume_data.type = vol->type();
    m_current_command_type = CommandType::ModelVolumeManipulation;
}

void UndoRedo::begin(ModelObject* mo)
{
    if (working())
        return;

    if (m_current_command_type != CommandType::None)
        throw std::runtime_error("Undo/Redo stack - attemted to nest commands.");

    unsigned int i=0;
    for (; i<m_model->objects.size(); ++i)
        if (m_model->objects[i] == mo)
            break;
    m_model_object_data.mo_idx = i;
    m_model_object_data.inst_num = mo->instances.size();
    m_current_command_type = CommandType::ModelObjectManipulation;
}


void UndoRedo::end() 
{
    if (working())
        return;

    if (m_current_command_type == CommandType::None)
        throw std::runtime_error("Undo/Redo stack - unexpected command end.");
        
    if (m_current_command_type == CommandType::ModelInstanceManipulation) {
        if (m_model->objects[m_model_instance_data.mo_idx]->instances.size() != m_model_instance_data.inst_num) {
            // the instance was deleted
            push(new RemoveInstance(m_model,
                                    m_model_instance_data.transformation,
                                    m_model_instance_data.mo_idx,
                                    m_model_instance_data.mi_idx));
        }
        else {
            // the transformation matrix was changed
            push(new Change(m_model->objects[m_model_instance_data.mo_idx]->instances[m_model_instance_data.mi_idx],
                            m_model_instance_data.transformation,
                            m_model->objects[m_model_instance_data.mo_idx]->instances[m_model_instance_data.mi_idx]->get_transformation()));
        }
    }

    if (m_current_command_type == CommandType::ModelObjectManipulation) {
        if (m_model->objects[m_model_object_data.mo_idx]->instances.size() > m_model_object_data.inst_num) {
            // an instance was added - the one at the end
            push(new AddInstance(m_model->objects[m_model_object_data.mo_idx]->instances.back(), m_model_object_data.mo_idx));
        }
    }

    if (m_current_command_type == CommandType::ModelVolumeManipulation) {
        if (m_model->objects[m_model_volume_data.mo_idx]->volumes.size() != m_model_volume_data.vols_num) {
            // the volume was deleted

        }
        else {
            // the transformation matrix, name or type was changed
            ModelVolume* mv = m_model->objects[m_model_volume_data.mo_idx]->volumes[m_model_volume_data.mv_idx];
            push(new Change(mv, m_model_volume_data.transformation, mv->get_transformation(), m_model_volume_data.name, mv->name, m_model_volume_data.type, mv->type()));
        }
    }

    m_current_command_type = CommandType::None;
}
*/

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void UndoRedo::push(Command* command) {
    if (m_batch_running)
        command->m_bound_to_previous = true;
    if (m_batch_start)
        m_batch_running = true;
    if (m_batch_running)
        command->m_description = m_batch_desc;

     m_stack.resize(m_index); // clears the redo part of the stack
     m_stack.emplace_back(command);
     m_index = m_stack.size();

     GUI::wxGetApp().plater()->canvas3D()->toolbar_update_undo_redo();
}


void UndoRedo::undo()
{
    do {
        if (!anything_to_undo())
            return;

        --m_index;
        m_stack[m_index]->undo();
    } while (m_stack[m_index]->m_bound_to_previous);
}

void UndoRedo::redo()
{
    do {
        if (!anything_to_redo())
            return;

        m_stack[m_index]->redo();
        ++m_index;
    } while (m_index < m_stack.size() && m_stack[m_index]->m_bound_to_previous);
}

} // namespace Slic3r
