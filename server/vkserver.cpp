#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <functional>
#include <system_error>

class scope_guard
{
public:
	scope_guard(std::function<void()> f);
	~scope_guard();
	void dismiss();

private:
	bool m_dismiss { false };
	std::function<void()> m_func{ };
};

struct Flag
{
	uint64_t running          :  1;
	uint64_t reset_connection :  1;
	uint64_t                  : 62;
};

static Flag s_flag;

inline scope_guard::scope_guard(std::function<void()> f)
	: m_func(std::move(f))
{
}

inline scope_guard::~scope_guard()
{
	if (!m_dismiss && m_func)
		m_func();
}

inline void scope_guard::dismiss()
{
	m_dismiss = true;
}

int listen_tcp()
{
	auto const sock = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		throw std::system_error(
			std::make_error_code(
				std::errc(errno)), "failed to create socket");

	::scope_guard guard([sock]{ ::close(sock); });

	int yes = 1;
	if (::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof yes) != 0)
		throw std::system_error(
			std::make_error_code(
				std::errc(errno)), "failed to set port reuse");

	::sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7770);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0)
		throw std::system_error(
			std::make_error_code(
				std::errc(errno)), "failed to bind address");

	if (::listen(sock, 1024) != 0)
		throw std::system_error(
			std::make_error_code(
				std::errc(errno)), "failed to listen");

	guard.dismiss();
	return sock;
}

int accept_one()
{
	auto const listen_sock = ::listen_tcp();

	::sockaddr_in addr{};
	socklen_t len = sizeof addr;
	auto const client_sock = ::accept(
		listen_sock, reinterpret_cast<sockaddr *>(&addr), &len);

	if (client_sock < 0)
		throw std::system_error(
			std::make_error_code(
				std::errc(errno)), "failed to accept");

	std::cout
		<< "new connection: " << inet_ntoa(addr.sin_addr)
		<< ':' << ntohs(addr.sin_port) << std::endl;

	::close(listen_sock);
	return client_sock;
}


inline void print_descriptor(uint64_t const descriptor)
{
	auto const p = reinterpret_cast<const uint8_t(*)[8]>(&descriptor);
	std::cout
		<< std::hex
		<< int(p[0][0])
		<< int(p[0][1])
		<< int(p[0][2])
		<< int(p[0][3])
		<< int(p[0][4])
		<< int(p[0][5])
		<< int(p[0][6])
		<< int(p[0][7])
		<< std::endl;
}

int open_hidg0()
{
	auto const fd = ::open("/dev/hidg0", O_RDWR);
	if (fd < 0)
		throw std::system_error(
			std::make_error_code(
				std::errc(errno)), "failed to open /dev/hidg0");

	return fd;
}

void handle_client(int const dev, int const sock)
{
	::fcntl(F_SETFL, sock, ::fcntl(F_GETFL, sock, O_NONBLOCK));

	fd_set rset;
	FD_ZERO(&rset);
	FD_SET(sock, &rset);
	auto const maxfdp1 = sock + 1;

	while (s_flag.running)
	{
		auto set = rset;
		timeval timeout{ 1, 0 };
		auto const n = ::select(maxfdp1, &set, nullptr, nullptr, &timeout);
		if (n > 0) while (true)
		{
			uint64_t descriptor;
			auto const nread = ::read(sock, &descriptor, sizeof descriptor);
			if (nread == sizeof descriptor)
				::write(dev, &descriptor, sizeof descriptor);
			else if(nread == 0)
				return;
			else if (nread > 0)
				std::abort();
			else if (EAGAIN == errno)
				break;
			else if (ECONNRESET == errno)
				return;
			else throw std::system_error(
				std::make_error_code(
					std::errc(errno)), "failed to read");
		}
		else if (n == 0)
		{
		}
		else
		{
		}

		if (s_flag.reset_connection) break;
	}
}

int main()
{
	try
	{
		::daemon(1, 0);

		std::signal(SIGTERM, [](int){ s_flag.running = 0; });

		std::signal(SIGRTMIN, [](int){ s_flag.reset_connection = 1; });

		auto const dev = ::open_hidg0();

		s_flag.running = 1;

		while (s_flag.running)
		{
			std::cout << "listenning..." << std::endl;
			auto const sock = ::accept_one();
			s_flag.reset_connection = 0;
			::handle_client(dev, sock);
			::close(sock);
			std::cout << "connection closed" << std::endl;
		}

		::close(dev);
	}
	catch (std::system_error const & e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
}
