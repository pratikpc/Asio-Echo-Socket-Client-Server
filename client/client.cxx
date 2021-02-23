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
   using boost::asio::async_write;
   try
   {
      char data[1024];
      for (;socket.is_open();)
      {
         const std::string confirmed{"Client says Hi"};
         std::cout << "Sent Data\n";
         co_await socket.async_write_some(
             boost::asio::buffer(confirmed, std::size(confirmed)), use_awaitable);

         std::size_t n =
             co_await socket.async_read_some(boost::asio::buffer(data), use_awaitable);
         // Add Null at end
         // This will ensure string gets displayed
         data[n < std::size(data) ? n : std::size(data) - 1] = '\0';
         std::cout << "Received Data: " << data << '\n';
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

   tcp::resolver resolver(executor);
   std::cout << "Start connect attempt\n";

   auto endpoints = co_await resolver.async_resolve({"127.0.0.1", "54321"}, use_awaitable);
   auto endpoint  = endpoints.begin()->endpoint();
   std::cout << "Trying to connect to " << endpoint << '\n';
   tcp::socket socket{executor, tcp::v4()};

   co_await socket.async_connect(endpoint, use_awaitable);
   std::cout << "Connection made " << endpoint << '\n';
   co_spawn(executor, echo(std::move(socket)), detached);
}

int main()
{
   using boost::asio::co_spawn;
   using boost::asio::detached;
   using boost::asio::io_context;

   try
   {
      io_context io_context(3);

      boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
      signals.async_wait([&](auto, auto) {
         std::cout << "Stopping server\n";
         io_context.stop();
      });

      co_spawn(io_context, listener(), detached);
      // Run the I/O service on the requested number of threads
      static auto constexpr threads = 3;

      std::vector<std::thread> v{};
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
