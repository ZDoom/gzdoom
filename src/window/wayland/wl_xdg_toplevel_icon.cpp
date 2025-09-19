#include "wl_xdg_toplevel_icon.hpp"

using namespace wayland;
using namespace wayland::detail;

const wl_interface* xdg_toplevel_icon_manager_v1_interface_destroy_request[0] = {
};

const wl_interface* xdg_toplevel_icon_manager_v1_interface_create_icon_request[1] = {
  &xdg_toplevel_icon_v1_interface,
};

const wl_interface* xdg_toplevel_icon_manager_v1_interface_set_icon_request[2] = {
  &xdg_toplevel_interface,
  &xdg_toplevel_icon_v1_interface,
};

const wl_interface* xdg_toplevel_icon_manager_v1_interface_icon_size_event[1] = {
  nullptr,
};

const wl_interface* xdg_toplevel_icon_manager_v1_interface_done_event[0] = {
};

const wl_message xdg_toplevel_icon_manager_v1_interface_requests[3] = {
  {
    "destroy",
    "",
    xdg_toplevel_icon_manager_v1_interface_destroy_request,
  },
  {
    "create_icon",
    "n",
    xdg_toplevel_icon_manager_v1_interface_create_icon_request,
  },
  {
    "set_icon",
    "o?o",
    xdg_toplevel_icon_manager_v1_interface_set_icon_request,
  },
};

const wl_message xdg_toplevel_icon_manager_v1_interface_events[2] = {
  {
    "icon_size",
    "i",
    xdg_toplevel_icon_manager_v1_interface_icon_size_event,
  },
  {
    "done",
    "",
    xdg_toplevel_icon_manager_v1_interface_done_event,
  },
};

const wl_interface wayland::detail::xdg_toplevel_icon_manager_v1_interface =
  {
    "xdg_toplevel_icon_manager_v1",
    1,
    3,
    xdg_toplevel_icon_manager_v1_interface_requests,
    2,
    xdg_toplevel_icon_manager_v1_interface_events,
  };

const wl_interface* xdg_toplevel_icon_v1_interface_destroy_request[0] = {
};

const wl_interface* xdg_toplevel_icon_v1_interface_set_name_request[1] = {
  nullptr,
};

const wl_interface* xdg_toplevel_icon_v1_interface_add_buffer_request[2] = {
  &buffer_interface,
  nullptr,
};

const wl_message xdg_toplevel_icon_v1_interface_requests[3] = {
  {
    "destroy",
    "",
    xdg_toplevel_icon_v1_interface_destroy_request,
  },
  {
    "set_name",
    "s",
    xdg_toplevel_icon_v1_interface_set_name_request,
  },
  {
    "add_buffer",
    "oi",
    xdg_toplevel_icon_v1_interface_add_buffer_request,
  },
};

const wl_message xdg_toplevel_icon_v1_interface_events[0] = {
};

const wl_interface wayland::detail::xdg_toplevel_icon_v1_interface =
  {
    "xdg_toplevel_icon_v1",
    1,
    3,
    xdg_toplevel_icon_v1_interface_requests,
    0,
    xdg_toplevel_icon_v1_interface_events,
  };

xdg_toplevel_icon_manager_v1_t::xdg_toplevel_icon_manager_v1_t(const proxy_t &p)
  : proxy_t(p)
{
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&xdg_toplevel_icon_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return xdg_toplevel_icon_manager_v1_t(p); });
}

xdg_toplevel_icon_manager_v1_t::xdg_toplevel_icon_manager_v1_t()
{
  set_interface(&xdg_toplevel_icon_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return xdg_toplevel_icon_manager_v1_t(p); });
}

xdg_toplevel_icon_manager_v1_t::xdg_toplevel_icon_manager_v1_t(xdg_toplevel_icon_manager_v1 *p, wrapper_type t)
  : proxy_t(reinterpret_cast<wl_proxy*> (p), t){
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&xdg_toplevel_icon_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return xdg_toplevel_icon_manager_v1_t(p); });
}

xdg_toplevel_icon_manager_v1_t::xdg_toplevel_icon_manager_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/)
  : proxy_t(wrapped_proxy, construct_proxy_wrapper_tag()){
  set_interface(&xdg_toplevel_icon_manager_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return xdg_toplevel_icon_manager_v1_t(p); });
}

xdg_toplevel_icon_manager_v1_t xdg_toplevel_icon_manager_v1_t::proxy_create_wrapper()
{
  return {*this, construct_proxy_wrapper_tag()};
}

const std::string xdg_toplevel_icon_manager_v1_t::interface_name = "xdg_toplevel_icon_manager_v1";

xdg_toplevel_icon_manager_v1_t::operator xdg_toplevel_icon_manager_v1*() const
{
  return reinterpret_cast<xdg_toplevel_icon_manager_v1*> (c_ptr());
}

xdg_toplevel_icon_v1_t xdg_toplevel_icon_manager_v1_t::create_icon()
{
  proxy_t p = marshal_constructor(1U, &xdg_toplevel_icon_v1_interface, nullptr);
  return xdg_toplevel_icon_v1_t(p);
}


void xdg_toplevel_icon_manager_v1_t::set_icon(xdg_toplevel_t const& toplevel, xdg_toplevel_icon_v1_t const& icon)
{
  marshal(2U, toplevel.proxy_has_object() ? reinterpret_cast<wl_object*>(toplevel.c_ptr()) : nullptr, icon.proxy_has_object() ? reinterpret_cast<wl_object*>(icon.c_ptr()) : nullptr);
}


std::function<void(int32_t)> &xdg_toplevel_icon_manager_v1_t::on_icon_size()
{
  return std::static_pointer_cast<events_t>(get_events())->icon_size;
}

std::function<void()> &xdg_toplevel_icon_manager_v1_t::on_done()
{
  return std::static_pointer_cast<events_t>(get_events())->done;
}

int xdg_toplevel_icon_manager_v1_t::dispatcher(uint32_t opcode, const std::vector<any>& args, const std::shared_ptr<detail::events_base_t>& e)
{
  std::shared_ptr<events_t> events = std::static_pointer_cast<events_t>(e);
  switch(opcode)
    {
    case 0:
      if(events->icon_size) events->icon_size(args[0].get<int32_t>());
      break;
    case 1:
      if(events->done) events->done();
      break;
    }
  return 0;
}

xdg_toplevel_icon_v1_t::xdg_toplevel_icon_v1_t(const proxy_t &p)
  : proxy_t(p)
{
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&xdg_toplevel_icon_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return xdg_toplevel_icon_v1_t(p); });
}

xdg_toplevel_icon_v1_t::xdg_toplevel_icon_v1_t()
{
  set_interface(&xdg_toplevel_icon_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return xdg_toplevel_icon_v1_t(p); });
}

xdg_toplevel_icon_v1_t::xdg_toplevel_icon_v1_t(xdg_toplevel_icon_v1 *p, wrapper_type t)
  : proxy_t(reinterpret_cast<wl_proxy*> (p), t){
  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)
    {
      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);
      set_destroy_opcode(0U);
    }
  set_interface(&xdg_toplevel_icon_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return xdg_toplevel_icon_v1_t(p); });
}

xdg_toplevel_icon_v1_t::xdg_toplevel_icon_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/)
  : proxy_t(wrapped_proxy, construct_proxy_wrapper_tag()){
  set_interface(&xdg_toplevel_icon_v1_interface);
  set_copy_constructor([] (const proxy_t &p) -> proxy_t
    { return xdg_toplevel_icon_v1_t(p); });
}

xdg_toplevel_icon_v1_t xdg_toplevel_icon_v1_t::proxy_create_wrapper()
{
  return {*this, construct_proxy_wrapper_tag()};
}

const std::string xdg_toplevel_icon_v1_t::interface_name = "xdg_toplevel_icon_v1";

xdg_toplevel_icon_v1_t::operator xdg_toplevel_icon_v1*() const
{
  return reinterpret_cast<xdg_toplevel_icon_v1*> (c_ptr());
}

void xdg_toplevel_icon_v1_t::set_name(std::string const& icon_name)
{
  marshal(1U, icon_name);
}


void xdg_toplevel_icon_v1_t::add_buffer(buffer_t const& buffer, int32_t scale)
{
  marshal(2U, buffer.proxy_has_object() ? reinterpret_cast<wl_object*>(buffer.c_ptr()) : nullptr, scale);
}


int xdg_toplevel_icon_v1_t::dispatcher(uint32_t opcode, const std::vector<any>& args, const std::shared_ptr<detail::events_base_t>& e)
{
  return 0;
}



