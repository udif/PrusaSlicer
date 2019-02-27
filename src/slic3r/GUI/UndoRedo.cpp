#include "UndoRedo.hpp"
#include "GUI_App.hpp"
#include "GUI_ObjectList.hpp"
#include "GLCanvas3D.hpp"

#define UNDOREDO_DEBUG 1

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

UndoRedo::ChangeVolumeType::ChangeVolumeType(Model* model, ModelVolume* vol, ModelVolumeType new_type)
: ChangeVolumeType(model, get_idx(vol->get_object()), get_idx(vol), new_type) {}

UndoRedo::ChangeVolumeType::ChangeVolumeType(Model* model, int mo_idx, int mv_idx, ModelVolumeType new_type)
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
UndoRedo::AddInstance::AddInstance(Model* model, ModelObject* mo, int mi_idx, const Geometry::Transformation& t)
: AddInstance(model, get_idx(mo), mi_idx, t) {}

UndoRedo::AddInstance::AddInstance(Model* model, int mo_idx, int mi_idx, const Geometry::Transformation& t)
: m_mo_idx(mo_idx),
  m_mi_idx(mi_idx == -1 ? model->objects[mo_idx]->instances.size() : mi_idx),
  m_transformation(t)
{
    m_model = model;
    m_description = "Add instance";
}

void UndoRedo::AddInstance::undo()
{
    ModelObject* mo = m_model->objects[m_mo_idx];
    delete (mo->instances[m_mi_idx]);
    mo->instances.erase(mo->instances.begin() + m_mi_idx);
    mo->invalidate_bounding_box();
}

void UndoRedo::AddInstance::redo()
{
    ModelObject* mo = m_model->objects[m_mo_idx];
    ModelInstance* i = new ModelInstance(mo);
    i->m_transformation = m_transformation;
    mo->instances.insert(mo->instances.begin() + m_mi_idx, i);
    mo->invalidate_bounding_box();
}

////////////////////////////////////////////////////////////////////

UndoRedo::RemoveInstance::RemoveInstance(Model* model, ModelObject* mo, int mi_idx)
: RemoveInstance(model, get_idx(mo), mi_idx) {}

UndoRedo::RemoveInstance::RemoveInstance(Model* model, int mo_idx, int mi_idx)
: m_command_add_instance(model, mo_idx, mi_idx, (mi_idx == -1 ? model->objects[mo_idx]->instances.back() : model->objects[mo_idx]->instances[mi_idx])->m_transformation)
{
    m_model = model;
    m_description = "Remove instance";
}

void UndoRedo::RemoveInstance::undo()
{
    m_command_add_instance.redo();
}

void UndoRedo::RemoveInstance::redo()
{
    m_command_add_instance.undo();
}

////////////////////////////////////////////////////////////////////
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
    if (m_batch_depth == 0)
        m_batch_desc = desc;
    ++m_batch_depth;

#ifdef UNDOREDO_DEBUG
    std::cout << "begin_batch (" << desc << ") : m_batch_depth=" << m_batch_depth << std::endl;
#endif

}


void UndoRedo::end_batch() {
    --m_batch_depth;
    if (m_batch_depth < 0)
        throw std::runtime_error("UndoRedo: extra end_batch call.");

    m_batch_running = (m_batch_depth > 0);
#ifdef UNDOREDO_DEBUG
    std::cout << "end_batch : m_batch_depth=" << m_batch_depth << std::endl;
#endif
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void UndoRedo::push(Command* command) {
    if (m_batch_running)
        command->m_bound_to_previous = true;
    if (m_batch_depth > 0)
        m_batch_running = true;
    if (m_batch_running)
        command->m_description = m_batch_desc;

     m_stack.resize(m_index); // clears the redo part of the stack
     m_stack.emplace_back(command);
     m_index = m_stack.size();

     GUI::wxGetApp().plater()->canvas3D()->toolbar_update_undo_redo();
     print_stack();
}


void UndoRedo::undo()
{
    do {
        if (!anything_to_undo())
            return;

        --m_index;
        m_stack[m_index]->undo();
    } while (m_stack[m_index]->m_bound_to_previous);
    print_stack();
}

void UndoRedo::redo()
{
    do {
        if (!anything_to_redo())
            return;

        m_stack[m_index]->redo();
        ++m_index;
    } while (m_index < m_stack.size() && m_stack[m_index]->m_bound_to_previous);
    print_stack();
}


void UndoRedo::print_stack() const
{
    std::cout << "=============================" << std::endl;
    for (unsigned int i = 0; i<m_stack.size(); ++i)
        std::cout << i << "\t" << (i==m_index ? "->" : "  ") << typeid(*(m_stack[i])).name()+19 << "\t" << m_stack[i]->m_bound_to_previous << std::endl;    
}

} // namespace Slic3r
