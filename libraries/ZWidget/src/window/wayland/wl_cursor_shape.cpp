#include "wl_cursor_shape.hpp"

using namespace wayland;
using namespace wayland::detail;

const wl_interface* cursor_shape_manager_v1_interface_destroy_request[0] = {
};

const wl_interface* cursor_shape_manager_v1_interface_get_pointer_request[2] = {
  &cursor_shape_device_v1_interface,
  &pointer_interface,
};

const wl_interface* cursor_shape_manager_v1_interface_get_tablet_tool_v2_request[2] = {
  &cursor_shape_device_v1_interface,
  &zwp_tablet_tool_v2_interface,
};

const wl_message cursor_shape_manager_v1_interface_requests[3] = {
  {
    "destroy",
    "",
    cursor_shape_manager_v1_interface_destroy_request,
  },
  {
    "get_pointer",
    "no",
    cursor_shape_manager_v1_interface_get_pointer_request,
  },
  {
    "get_tablet_tool_v2",
    "no",
    cursor_shape_manager_v1_interface_get_tablet_tool_v2_request,
  },
};

const wl_message cursor_shape_manager_v1_interface_events[0] = {
};

const wl_interface wayland::detail::cursor_shape_manager_v1_interface =
  {
    "wp_cursor_shape_manager_v1",
    2,
    3,
    cursor_shape_manager_v1_interface_requests,
    0,
    cursor_shape_manager_v1_interface_events,
  };

const wl_interface* cursor_shape_device_v1_interface_destroy_request[0] = {
};

const wl_interface* cursor_shape_device_v1_interface_set_shape_request[2] = {
  nullptr,
  nullptr,
};

const wl_message cursor_shape_device_v1_interface_requests[2] = {
  {
    "destroy",
    "",
    cursor_shape_device_v1_interface_destroy_request,
  },
  {
    "set_shape",
    "uu",
    cursor_shape_device_v1_interface_set_shape_request,
  },
};

const wl_message cursor_shape_device_v1_interface_events[0] = {
};

const wl_interface wayland::detail::cursor_shape_device_v1_interface =
  {
    "wp_cursor_shape_device_v1",
    2,
    2,
    cursor_shape_device_v1_interface_requests,
    0,
    cursor_shape_device_v1_interface_events,
  };

cursor_shape_manager_v1_t::cursor_shape_manager_v1_t(const proxy_t &p)
  : proxy_t(p)
{
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&cursor_shape_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return cursor_shape_manager_v1_t(p); });
}

cursor_shape_manager_v1_t::cursor_shape_manager_v1_t()
{
  set_interface(&cursor_shape_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return cursor_shape_manager_v1_t(p); });
}

cursor_shape_manager_v1_t::cursor_shape_manager_v1_t(wp_cursor_shape_manager_v1 *p, wrapper_type t)
  : proxy_t(reinterpret_cast<wl_proxy*> (p), t){
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&cursor_shape_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return cursor_shape_manager_v1_t(p); });
}

cursor_shape_manager_v1_t::cursor_shape_manager_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/)
  : proxy_t(wrapped_proxy, construct_proxy_wrapper_tag()){
  set_interface(&cursor_shape_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return cursor_shape_manager_v1_t(p); });
}

cursor_shape_manager_v1_t cursor_shape_manager_v1_t::proxy_create_wrapper()
{
  return {*this, construct_proxy_wrapper_tag()};
}

const std::string cursor_shape_manager_v1_t::interface_name = "wp_cursor_shape_manager_v1";

cursor_shape_manager_v1_t::operator wp_cursor_shape_manager_v1*() const
{
  return reinterpret_cast<wp_cursor_shape_manager_v1*> (c_ptr());
}

cursor_shape_device_v1_t cursor_shape_manager_v1_t::get_pointer(pointer_t const& pointer)
{
  proxy_t p = marshal_constructor(1U, &cursor_shape_device_v1_interface, nullptr, pointer.proxy_has_object() ? reinterpret_cast<wl_object*>(pointer.c_ptr()) : nullptr);
  return cursor_shape_device_v1_t(p);
}


cursor_shape_device_v1_t cursor_shape_manager_v1_t::get_tablet_tool_v2(zwp_tablet_tool_v2_t const& tablet_tool)
{
  proxy_t p = marshal_constructor(2U, &cursor_shape_device_v1_interface, nullptr, tablet_tool.proxy_has_object() ? reinterpret_cast<wl_object*>(tablet_tool.c_ptr()) : nullptr);
  return cursor_shape_device_v1_t(p);
}


int cursor_shape_manager_v1_t::dispatcher(uint32_t opcode, const std::vector<any>& args, const std::shared_ptr<detail::events_base_t>& e)
{
  return 0;
}

cursor_shape_device_v1_t::cursor_shape_device_v1_t(const proxy_t &p)
  : proxy_t(p)
{
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&cursor_shape_device_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return cursor_shape_device_v1_t(p); });
}

cursor_shape_device_v1_t::cursor_shape_device_v1_t()
{
  set_interface(&cursor_shape_device_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return cursor_shape_device_v1_t(p); });
}

cursor_shape_device_v1_t::cursor_shape_device_v1_t(wp_cursor_shape_device_v1 *p, wrapper_type t)
  : proxy_t(reinterpret_cast<wl_proxy*> (p), t){
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&cursor_shape_device_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return cursor_shape_device_v1_t(p); });
}

cursor_shape_device_v1_t::cursor_shape_device_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/)
  : proxy_t(wrapped_proxy, construct_proxy_wrapper_tag()){
  set_interface(&cursor_shape_device_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return cursor_shape_device_v1_t(p); });
}

cursor_shape_device_v1_t cursor_shape_device_v1_t::proxy_create_wrapper()
{
  return {*this, construct_proxy_wrapper_tag()};
}

const std::string cursor_shape_device_v1_t::interface_name = "wp_cursor_shape_device_v1";

cursor_shape_device_v1_t::operator wp_cursor_shape_device_v1*() const
{
  return reinterpret_cast<wp_cursor_shape_device_v1*> (c_ptr());
}

void cursor_shape_device_v1_t::set_shape(uint32_t serial, cursor_shape_device_v1_shape const& shape)
{
  marshal(1U, serial, static_cast<uint32_t>(shape));
}


int cursor_shape_device_v1_t::dispatcher(uint32_t opcode, const std::vector<any>& args, const std::shared_ptr<detail::events_base_t>& e)
{
  return 0;
}




