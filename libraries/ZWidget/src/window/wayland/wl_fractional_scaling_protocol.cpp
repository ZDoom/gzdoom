#include "wl_fractional_scaling_protocol.hpp"

using namespace wayland;
using namespace wayland::detail;

const wl_interface* fractional_scale_manager_v1_interface_destroy_request[0] = {
};

const wl_interface* fractional_scale_manager_v1_interface_get_fractional_scale_request[2] = {
  &fractional_scale_v1_interface,
  &surface_interface,
};

const wl_message fractional_scale_manager_v1_interface_requests[2] = {
  {
    "destroy",
    "",
    fractional_scale_manager_v1_interface_destroy_request,
  },
  {
    "get_fractional_scale",
    "no",
    fractional_scale_manager_v1_interface_get_fractional_scale_request,
  },
};

const wl_message fractional_scale_manager_v1_interface_events[0] = {
};

const wl_interface wayland::detail::fractional_scale_manager_v1_interface =
  {
    "wp_fractional_scale_manager_v1",
    1,
    2,
    fractional_scale_manager_v1_interface_requests,
    0,
    fractional_scale_manager_v1_interface_events,
  };

const wl_interface* fractional_scale_v1_interface_destroy_request[0] = {
};

const wl_interface* fractional_scale_v1_interface_preferred_scale_event[1] = {
  nullptr,
};

const wl_message fractional_scale_v1_interface_requests[1] = {
  {
    "destroy",
    "",
    fractional_scale_v1_interface_destroy_request,
  },
};

const wl_message fractional_scale_v1_interface_events[1] = {
  {
    "preferred_scale",
    "u",
    fractional_scale_v1_interface_preferred_scale_event,
  },
};

const wl_interface wayland::detail::fractional_scale_v1_interface =
  {
    "wp_fractional_scale_v1",
    1,
    1,
    fractional_scale_v1_interface_requests,
    1,
    fractional_scale_v1_interface_events,
  };

fractional_scale_manager_v1_t::fractional_scale_manager_v1_t(const proxy_t &p)
  : proxy_t(p)
{
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&fractional_scale_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return fractional_scale_manager_v1_t(p); });
}

fractional_scale_manager_v1_t::fractional_scale_manager_v1_t()
{
  set_interface(&fractional_scale_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return fractional_scale_manager_v1_t(p); });
}

fractional_scale_manager_v1_t::fractional_scale_manager_v1_t(wp_fractional_scale_manager_v1 *p, wrapper_type t)
  : proxy_t(reinterpret_cast<wl_proxy*> (p), t){
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&fractional_scale_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return fractional_scale_manager_v1_t(p); });
}

fractional_scale_manager_v1_t::fractional_scale_manager_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/)
  : proxy_t(wrapped_proxy, construct_proxy_wrapper_tag()){
  set_interface(&fractional_scale_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return fractional_scale_manager_v1_t(p); });
}

fractional_scale_manager_v1_t fractional_scale_manager_v1_t::proxy_create_wrapper()
{
  return {*this, construct_proxy_wrapper_tag()};
}

const std::string fractional_scale_manager_v1_t::interface_name = "wp_fractional_scale_manager_v1";

fractional_scale_manager_v1_t::operator wp_fractional_scale_manager_v1*() const
{
  return reinterpret_cast<wp_fractional_scale_manager_v1*> (c_ptr());
}

fractional_scale_v1_t fractional_scale_manager_v1_t::get_fractional_scale(surface_t const& surface)
{
  proxy_t p = marshal_constructor(1U, &fractional_scale_v1_interface, nullptr, surface.proxy_has_object() ? reinterpret_cast<wl_object*>(surface.c_ptr()) : nullptr);
  return fractional_scale_v1_t(p);
}


int fractional_scale_manager_v1_t::dispatcher(uint32_t opcode, const std::vector<any>& args, const std::shared_ptr<detail::events_base_t>& e)
{
  return 0;
}


fractional_scale_v1_t::fractional_scale_v1_t(const proxy_t &p)
  : proxy_t(p)
{
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&fractional_scale_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return fractional_scale_v1_t(p); });
}

fractional_scale_v1_t::fractional_scale_v1_t()
{
  set_interface(&fractional_scale_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return fractional_scale_v1_t(p); });
}

fractional_scale_v1_t::fractional_scale_v1_t(wp_fractional_scale_v1 *p, wrapper_type t)
  : proxy_t(reinterpret_cast<wl_proxy*> (p), t){
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&fractional_scale_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return fractional_scale_v1_t(p); });
}

fractional_scale_v1_t::fractional_scale_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/)
  : proxy_t(wrapped_proxy, construct_proxy_wrapper_tag()){
  set_interface(&fractional_scale_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return fractional_scale_v1_t(p); });
}

fractional_scale_v1_t fractional_scale_v1_t::proxy_create_wrapper()
{
  return {*this, construct_proxy_wrapper_tag()};
}

const std::string fractional_scale_v1_t::interface_name = "wp_fractional_scale_v1";

fractional_scale_v1_t::operator wp_fractional_scale_v1*() const
{
  return reinterpret_cast<wp_fractional_scale_v1*> (c_ptr());
}

std::function<void(uint32_t)> &fractional_scale_v1_t::on_preferred_scale()
{
  return std::static_pointer_cast<events_t>(get_events())->preferred_scale;
}

int fractional_scale_v1_t::dispatcher(uint32_t opcode, const std::vector<any>& args, const std::shared_ptr<detail::events_base_t>& e)
{
  std::shared_ptr<events_t> events = std::static_pointer_cast<events_t>(e);
  switch(opcode)
    {
    case 0:
      if(events->preferred_scale) events->preferred_scale(args[0].get<uint32_t>());
      break;
    }
  return 0;
}


