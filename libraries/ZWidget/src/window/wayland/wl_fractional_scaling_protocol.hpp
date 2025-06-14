#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wayland-client.hpp>

struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;

namespace wayland
{
class fractional_scale_manager_v1_t;
enum class fractional_scale_manager_v1_error : uint32_t;
class fractional_scale_v1_t;

namespace detail
{
  extern const wl_interface fractional_scale_manager_v1_interface;
  extern const wl_interface fractional_scale_v1_interface;
}

/** \brief fractional surface scale information

      A global interface for requesting surfaces to use fractional scales.
    
*/
class fractional_scale_manager_v1_t : public proxy_t
{
private:
  struct events_t : public detail::events_base_t
  {
  };

  static int dispatcher(uint32_t opcode, const std::vector<detail::any>& args, const std::shared_ptr<detail::events_base_t>& e);

  fractional_scale_manager_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);

public:
  fractional_scale_manager_v1_t();
  explicit fractional_scale_manager_v1_t(const proxy_t &proxy);
  fractional_scale_manager_v1_t(wp_fractional_scale_manager_v1 *p, wrapper_type t = wrapper_type::standard);

  fractional_scale_manager_v1_t proxy_create_wrapper();

  static const std::string interface_name;

  operator wp_fractional_scale_manager_v1*() const;

  /** \brief extend surface interface for scale information
      \return the new surface scale info interface id
      \param surface the surface

        Create an add-on object for the the wl_surface to let the compositor
        request fractional scales. If the given wl_surface already has a
        wp_fractional_scale_v1 object associated, the fractional_scale_exists
        protocol error is raised.
      
  */
  fractional_scale_v1_t get_fractional_scale(surface_t const& surface);

  /** \brief Minimum protocol version required for the \ref get_fractional_scale function
  */
  static constexpr std::uint32_t get_fractional_scale_since_version = 1;

};

/** \brief 

  */
enum class fractional_scale_manager_v1_error : uint32_t
  {
  /** \brief the surface already has a fractional_scale object associated */
  fractional_scale_exists = 0
};


/** \brief fractional scale interface to a wl_surface

      An additional interface to a wl_surface object which allows the compositor
      to inform the client of the preferred scale.
    
*/
class fractional_scale_v1_t : public proxy_t
{
private:
  struct events_t : public detail::events_base_t
  {
    std::function<void(uint32_t)> preferred_scale;
  };

  static int dispatcher(uint32_t opcode, const std::vector<detail::any>& args, const std::shared_ptr<detail::events_base_t>& e);

  fractional_scale_v1_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);

public:
  fractional_scale_v1_t();
  explicit fractional_scale_v1_t(const proxy_t &proxy);
  fractional_scale_v1_t(wp_fractional_scale_v1 *p, wrapper_type t = wrapper_type::standard);

  fractional_scale_v1_t proxy_create_wrapper();

  static const std::string interface_name;

  operator wp_fractional_scale_v1*() const;

  /** \brief notify of new preferred scale
      \param scale the new preferred scale

        Notification of a new preferred scale for this surface that the
        compositor suggests that the client should use.

        The sent scale is the numerator of a fraction with a denominator of 120.
      
  */
  std::function<void(uint32_t)> &on_preferred_scale();

};



}
