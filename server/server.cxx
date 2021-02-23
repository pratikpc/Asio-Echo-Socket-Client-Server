#define BOOST_ASIO_HAS_STD_COROUTINE
#define BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>

#include <iostream>
#include <thread>

using boost::asio::awaitable;
using boost::asio::detached;
using boost::asio::use_awaitable;
namespace this_coro = boost::asio::this_coro;

using boost::asio::ip::tcp;

awaitable<void> echo(tcp::socket socket)
{
   try
   {
      char data[1024];
      for (; socket.is_open();)
      {
         std::cout << "Waiting to receive data\n";
         std::size_t n =
             co_await socket.async_read_some(boost::asio::buffer(data), use_awaitable);
         // Add Null at end
         // This will ensure string gets displayed
         data[n < std::size(data) ? n : std::size(data) - 1] = '\0';
         std::cout << "Received Data: " << data << '\n';

         co_await async_write(socket, boost::asio::buffer(data, n), use_awaitable);
         const std::string confirmed{"Server sends it's confirmational regards"};
         co_await async_write(
             socket, boost::asio::buffer(confirmed, std::size(confirmed)), use_awaitable);
      }
   }
   catch (std::exception& e)
   {
      std::printf("echo Exception: %s\n", e.what());
   }
}

awaitable<void> listener()
{
   auto executor = co_await this_coro::executor;

   tcp::acceptor acceptor(executor, {tcp::v4(), 54321});
   for (;;)
   {
      std::cout << "Server running at " << acceptor.local_endpoint() << "\n";
      std::cout << "Waiting for a connection\n";
      tcp::socket socket = co_await acceptor.async_accept(use_awaitable);
      std::cout << "Received connection\n";
      co_spawn(executor, echo(std::move(socket)), detached);
   }
}

int main()
{
   try
   {
      boost::asio::io_context io_context(3);

      boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto) {
         std::cout << "Stopping server\n";
         io_context.stop();
      });

      boost::asio::co_spawn(io_context, listener(), boost::asio::detached);
      // Run the I/O service on the requested number of threads
      static auto constexpr threads = 3;

      std::vector<std::thread> v;
      v.reserve(threads - 1);
      for (auto i = threads - 1; i > 0; --i)
         v.emplace_back([&io_context] {
            std::cout << std::this_thread::get_id() << " started\n";
            io_context.run();
            std::cout << std::this_thread::get_id() << " over\n";
         });
      std::cout << std::this_thread::get_id() << " started\n";
      io_context.run();
      std::cout << std::this_thread::get_id() << " overed\n";
      for (auto& thread : v)
         thread.join();
   }
   catch (...)
   {
      std::cout << "Exception";
   }
}
