#ifndef PROXY_SERVER_CONNECTION_MANAGER_HPP
#define PROXY_SERVER_CONNECTION_MANAGER_HPP

#include "connection.hpp"
#include <boost/noncopyable.hpp>
#include <boost/unordered_set.hpp>
#include <set>

namespace proxy {
namespace server {

/// Manages open connections so that they may be cleanly stopped when the server
/// needs to shut down.
class connection_manager : private boost::noncopyable {
public:
  /// Add the specified connection to the manager and start it.
  void start(connection_ptr c);

  /// Stop the specified connection.
  void stop(connection_ptr c);

  /// Stop all connections.
  void stop_all();

private:
  /// The managed connections.
  boost::unordered_set<connection_ptr> connections_{};
};

} // namespace server
} // namespace proxy

#endif // PROXY_SERVER_CONNECTION_MANAGER_HPP
