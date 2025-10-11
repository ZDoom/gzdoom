#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wayland-client.hpp>
#include <wayland-client-protocol-unstable.hpp>

struct wp_cursor_shape_manager_v1;
struct wp_cursor_shape_device_v1;

namespace wayland
{
class cursor_shape_manager_v1_t;
class cursor_shape_device_v1_t;
enum class cursor_shape_device_v1_shape : uint32_t;
enum class cursor_shape_device_v1_error : uint32_t;

namespace detail
{
  extern const wl_interface cursor_shape_manager_v1_interface;
  extern const wl_interface cursor_shape_device_v1_interface;
}

/** \brief cursor shape manager

      This global offers an alternative, optional way to set cursor images. This
      new way uses enumerated cursors instead of a wl_surface like
      wl_pointer.set_cursor does.

      Warning! The protocol described in this file is currently in the testing
      phase. Backward compatible changes may be added together with the
      corresponding interface version bump. Backward incompatible changes can
      only be done by creating a new major version of the extension.
    
*/
class cursor_shape_manager_v1_t : public proxy_t
{
private:
  struct events_t : public detail::events_base_t
  {
  };

  static int dispatcher(uint32_t opcode, const std::vector<detail::any>& args, const std::shared_ptr<detail::events_base_t>& e);

  cursor_shape_manager_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);

public:
  cursor_shape_manager_v1_t();
  explicit cursor_shape_manager_v1_t(const proxy_t &proxy);
  cursor_shape_manager_v1_t(wp_cursor_shape_manager_v1 *p, wrapper_type t = wrapper_type::standard);

  cursor_shape_manager_v1_t proxy_create_wrapper();

  static const std::string interface_name;

  operator wp_cursor_shape_manager_v1*() const;

  /** \brief manage the cursor shape of a pointer device
      \param pointer 

        Obtain a wp_cursor_shape_device_v1 for a wl_pointer object.

        When the pointer capability is removed from the wl_seat, the
        wp_cursor_shape_device_v1 object becomes inert.
      
  */
  cursor_shape_device_v1_t get_pointer(pointer_t const& pointer);

  /** \brief Minimum protocol version required for the \ref get_pointer function
  */
  static constexpr std::uint32_t get_pointer_since_version = 1;

  /** \brief manage the cursor shape of a tablet tool device
      \param tablet_tool 

        Obtain a wp_cursor_shape_device_v1 for a zwp_tablet_tool_v2 object.

        When the zwp_tablet_tool_v2 is removed, the wp_cursor_shape_device_v1
        object becomes inert.
      
  */
  cursor_shape_device_v1_t get_tablet_tool_v2(zwp_tablet_tool_v2_t const& tablet_tool);

  /** \brief Minimum protocol version required for the \ref get_tablet_tool_v2 function
  */
  static constexpr std::uint32_t get_tablet_tool_v2_since_version = 1;

};


/** \brief cursor shape for a device

      This interface allows clients to set the cursor shape.
    
*/
class cursor_shape_device_v1_t : public proxy_t
{
private:
  struct events_t : public detail::events_base_t
  {
  };

  static int dispatcher(uint32_t opcode, const std::vector<detail::any>& args, const std::shared_ptr<detail::events_base_t>& e);

  cursor_shape_device_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);

public:
  cursor_shape_device_v1_t();
  explicit cursor_shape_device_v1_t(const proxy_t &proxy);
  cursor_shape_device_v1_t(wp_cursor_shape_device_v1 *p, wrapper_type t = wrapper_type::standard);

  cursor_shape_device_v1_t proxy_create_wrapper();

  static const std::string interface_name;

  operator wp_cursor_shape_device_v1*() const;

  /** \brief set device cursor to the shape
      \param serial serial number of the enter event
      \param shape 

        Sets the device cursor to the specified shape. The compositor will
        change the cursor image based on the specified shape.

        The cursor actually changes only if the input device focus is one of
        the requesting client's surfaces. If any, the previous cursor image
        (surface or shape) is replaced.

        The "shape" argument must be a valid enum entry, otherwise the
        invalid_shape protocol error is raised.

        This is similar to the wl_pointer.set_cursor and
        zwp_tablet_tool_v2.set_cursor requests, but this request accepts a
        shape instead of contents in the form of a surface. Clients can mix
        set_cursor and set_shape requests.

        The serial parameter must match the latest wl_pointer.enter or
        zwp_tablet_tool_v2.proximity_in serial number sent to the client.
        Otherwise the request will be ignored.
      
  */
  void set_shape(uint32_t serial, cursor_shape_device_v1_shape const& shape);

  /** \brief Minimum protocol version required for the \ref set_shape function
  */
  static constexpr std::uint32_t set_shape_since_version = 1;

};

/** \brief cursor shapes

        This enum describes cursor shapes.

        The names are taken from the CSS W3C specification:
        https://w3c.github.io/csswg-drafts/css-ui/#cursor
        with a few additions.

        Note that there are some groups of cursor shapes that are related:
        The first group is drag-and-drop cursors which are used to indicate
        the selected action during dnd operations. The second group is resize
        cursors which are used to indicate resizing and moving possibilities
        on window borders. It is recommended that the shapes in these groups
        should use visually compatible images and metaphors.
      
  */
enum class cursor_shape_device_v1_shape : uint32_t
  {
  /** \brief default cursor */
  _default = 1,
  /** \brief a context menu is available for the object under the cursor */
  context_menu = 2,
  /** \brief help is available for the object under the cursor */
  help = 3,
  /** \brief pointer that indicates a link or another interactive element */
  pointer = 4,
  /** \brief progress indicator */
  progress = 5,
  /** \brief program is busy, user should wait */
  wait = 6,
  /** \brief a cell or set of cells may be selected */
  cell = 7,
  /** \brief simple crosshair */
  crosshair = 8,
  /** \brief text may be selected */
  text = 9,
  /** \brief vertical text may be selected */
  vertical_text = 10,
  /** \brief drag-and-drop: alias of/shortcut to something is to be created */
  alias = 11,
  /** \brief drag-and-drop: something is to be copied */
  copy = 12,
  /** \brief drag-and-drop: something is to be moved */
  move = 13,
  /** \brief drag-and-drop: the dragged item cannot be dropped at the current cursor location */
  no_drop = 14,
  /** \brief drag-and-drop: the requested action will not be carried out */
  not_allowed = 15,
  /** \brief drag-and-drop: something can be grabbed */
  grab = 16,
  /** \brief drag-and-drop: something is being grabbed */
  grabbing = 17,
  /** \brief resizing: the east border is to be moved */
  e_resize = 18,
  /** \brief resizing: the north border is to be moved */
  n_resize = 19,
  /** \brief resizing: the north-east corner is to be moved */
  ne_resize = 20,
  /** \brief resizing: the north-west corner is to be moved */
  nw_resize = 21,
  /** \brief resizing: the south border is to be moved */
  s_resize = 22,
  /** \brief resizing: the south-east corner is to be moved */
  se_resize = 23,
  /** \brief resizing: the south-west corner is to be moved */
  sw_resize = 24,
  /** \brief resizing: the west border is to be moved */
  w_resize = 25,
  /** \brief resizing: the east and west borders are to be moved */
  ew_resize = 26,
  /** \brief resizing: the north and south borders are to be moved */
  ns_resize = 27,
  /** \brief resizing: the north-east and south-west corners are to be moved */
  nesw_resize = 28,
  /** \brief resizing: the north-west and south-east corners are to be moved */
  nwse_resize = 29,
  /** \brief resizing: that the item/column can be resized horizontally */
  col_resize = 30,
  /** \brief resizing: that the item/row can be resized vertically */
  row_resize = 31,
  /** \brief something can be scrolled in any direction */
  all_scroll = 32,
  /** \brief something can be zoomed in */
  zoom_in = 33,
  /** \brief something can be zoomed out */
  zoom_out = 34,
  /** \brief drag-and-drop: the user will select which action will be carried out (non-css value) */
  dnd_ask = 35,
  /** \brief resizing: something can be moved or resized in any direction (non-css value) */
  all_resize = 36
};

/** \brief 

  */
enum class cursor_shape_device_v1_error : uint32_t
  {
  /** \brief the specified shape value is invalid */
  invalid_shape = 1
};



}
