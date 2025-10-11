#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wayland-client.hpp>
#include <wayland-client-protocol-extra.hpp>

struct xdg_toplevel_icon_manager_v1;
struct xdg_toplevel_icon_v1;

namespace wayland
{
class xdg_toplevel_icon_manager_v1_t;
class xdg_toplevel_icon_v1_t;
enum class xdg_toplevel_icon_v1_error : uint32_t;

namespace detail
{
  extern const wl_interface xdg_toplevel_icon_manager_v1_interface;
  extern const wl_interface xdg_toplevel_icon_v1_interface;
}

/** \brief interface to manage toplevel icons

      This interface allows clients to create toplevel window icons and set
      them on toplevel windows to be displayed to the user.
    
*/
class xdg_toplevel_icon_manager_v1_t : public proxy_t
{
private:
  struct events_t : public detail::events_base_t
  {
    std::function<void(int32_t)> icon_size;
    std::function<void()> done;
  };

  static int dispatcher(uint32_t opcode, const std::vector<detail::any>& args, const std::shared_ptr<detail::events_base_t>& e);

  xdg_toplevel_icon_manager_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);

public:
  xdg_toplevel_icon_manager_v1_t();
  explicit xdg_toplevel_icon_manager_v1_t(const proxy_t &proxy);
  xdg_toplevel_icon_manager_v1_t(xdg_toplevel_icon_manager_v1 *p, wrapper_type t = wrapper_type::standard);

  xdg_toplevel_icon_manager_v1_t proxy_create_wrapper();

  static const std::string interface_name;

  operator xdg_toplevel_icon_manager_v1*() const;

  /** \brief create a new icon instance

        Creates a new icon object. This icon can then be attached to a
        xdg_toplevel via the 'set_icon' request.
      
  */
  xdg_toplevel_icon_v1_t create_icon();

  /** \brief Minimum protocol version required for the \ref create_icon function
  */
  static constexpr std::uint32_t create_icon_since_version = 1;

  /** \brief set an icon on a toplevel window
      \param toplevel the toplevel to act on
      \param icon 

        This request assigns the icon 'icon' to 'toplevel', or clears the
        toplevel icon if 'icon' was null.
        This state is double-buffered and is applied on the next
        wl_surface.commit of the toplevel.

        After making this call, the xdg_toplevel_icon_v1 provided as 'icon'
        can be destroyed by the client without 'toplevel' losing its icon.
        The xdg_toplevel_icon_v1 is immutable from this point, and any
        future attempts to change it must raise the
        'xdg_toplevel_icon_v1.immutable' protocol error.

        The compositor must set the toplevel icon from either the pixel data
        the icon provides, or by loading a stock icon using the icon name.
        See the description of 'xdg_toplevel_icon_v1' for details.

        If 'icon' is set to null, the icon of the respective toplevel is reset
        to its default icon (usually the icon of the application, derived from
        its desktop-entry file, or a placeholder icon).
        If this request is passed an icon with no pixel buffers or icon name
        assigned, the icon must be reset just like if 'icon' was null.
      
  */
  void set_icon(xdg_toplevel_t const& toplevel, xdg_toplevel_icon_v1_t const& icon);

  /** \brief Minimum protocol version required for the \ref set_icon function
  */
  static constexpr std::uint32_t set_icon_since_version = 1;

  /** \brief describes a supported & preferred icon size
      \param size the edge size of the square icon in surface-local coordinates, e.g. 64

        This event indicates an icon size the compositor prefers to be
        available if the client has scalable icons and can render to any size.

        When the 'xdg_toplevel_icon_manager_v1' object is created, the
        compositor may send one or more 'icon_size' events to describe the list
        of preferred icon sizes. If the compositor has no size preference, it
        may not send any 'icon_size' event, and it is up to the client to
        decide a suitable icon size.

        A sequence of 'icon_size' events must be finished with a 'done' event.
        If the compositor has no size preferences, it must still send the
        'done' event, without any preceding 'icon_size' events.
      
  */
  std::function<void(int32_t)> &on_icon_size();

  /** \brief all information has been sent

        This event is sent after all 'icon_size' events have been sent.
      
  */
  std::function<void()> &on_done();

};


/** \brief a toplevel window icon

      This interface defines a toplevel icon.
      An icon can have a name, and multiple buffers.
      In order to be applied, the icon must have either a name, or at least
      one buffer assigned. Applying an empty icon (with no buffer or name) to
      a toplevel should reset its icon to the default icon.

      It is up to compositor policy whether to prefer using a buffer or loading
      an icon via its name. See 'set_name' and 'add_buffer' for details.
    
*/
class xdg_toplevel_icon_v1_t : public proxy_t
{
private:
  struct events_t : public detail::events_base_t
  {
  };

  static int dispatcher(uint32_t opcode, const std::vector<detail::any>& args, const std::shared_ptr<detail::events_base_t>& e);

  xdg_toplevel_icon_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);

public:
  xdg_toplevel_icon_v1_t();
  explicit xdg_toplevel_icon_v1_t(const proxy_t &proxy);
  xdg_toplevel_icon_v1_t(xdg_toplevel_icon_v1 *p, wrapper_type t = wrapper_type::standard);

  xdg_toplevel_icon_v1_t proxy_create_wrapper();

  static const std::string interface_name;

  operator xdg_toplevel_icon_v1*() const;

  /** \brief set an icon name
      \param icon_name 

        This request assigns an icon name to this icon.
        Any previously set name is overridden.

        The compositor must resolve 'icon_name' according to the lookup rules
        described in the XDG icon theme specification[1] using the
        environment's current icon theme.

        If the compositor does not support icon names or cannot resolve
        'icon_name' according to the XDG icon theme specification it must
        fall back to using pixel buffer data instead.

        If this request is made after the icon has been assigned to a toplevel
        via 'set_icon', a 'immutable' error must be raised.

        [1]: https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html
      
  */
  void set_name(std::string const& icon_name);

  /** \brief Minimum protocol version required for the \ref set_name function
  */
  static constexpr std::uint32_t set_name_since_version = 1;

  /** \brief add icon data from a pixel buffer
      \param buffer 
      \param scale the scaling factor of the icon, e.g. 1

        This request adds pixel data supplied as wl_buffer to the icon.

        The client should add pixel data for all icon sizes and scales that
        it can provide, or which are explicitly requested by the compositor
        via 'icon_size' events on xdg_toplevel_icon_manager_v1.

        The wl_buffer supplying pixel data as 'buffer' must be backed by wl_shm
        and must be a square (width and height being equal).
        If any of these buffer requirements are not fulfilled, a 'invalid_buffer'
        error must be raised.

        If this icon instance already has a buffer of the same size and scale
        from a previous 'add_buffer' request, data from the last request
        overrides the preexisting pixel data.

        The wl_buffer must be kept alive for as long as the xdg_toplevel_icon
        it is associated with is not destroyed, otherwise a 'no_buffer' error
        is raised. The buffer contents must not be modified after it was
        assigned to the icon. As a result, the region of the wl_shm_pool's
        backing storage used for the wl_buffer must not be modified after this
        request is sent. The wl_buffer.release event is unused.

        If this request is made after the icon has been assigned to a toplevel
        via 'set_icon', a 'immutable' error must be raised.
      
  */
  void add_buffer(buffer_t const& buffer, int32_t scale);

  /** \brief Minimum protocol version required for the \ref add_buffer function
  */
  static constexpr std::uint32_t add_buffer_since_version = 1;

};

/** \brief 

  */
enum class xdg_toplevel_icon_v1_error : uint32_t
  {
  /** \brief the provided buffer does not satisfy requirements */
  invalid_buffer = 1,
  /** \brief the icon has already been assigned to a toplevel and must not be changed */
  immutable = 2,
  /** \brief the provided buffer has been destroyed before the toplevel icon */
  no_buffer = 3
};



}
