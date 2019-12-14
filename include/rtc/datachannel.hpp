/**
 * Copyright (c) 2019 Paul-Louis Ageneau
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef RTC_DATA_CHANNEL_H
#define RTC_DATA_CHANNEL_H

#include "channel.hpp"
#include "include.hpp"
#include "message.hpp"
#include "queue.hpp"
#include "reliability.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <type_traits>
#include <variant>

namespace rtc {

class SctpTransport;
class PeerConnection;

class DataChannel : public Channel {
public:
	DataChannel(std::shared_ptr<PeerConnection> pc, unsigned int stream, string label,
	            string protocol, Reliability reliability);
	DataChannel(std::shared_ptr<PeerConnection> pc, std::shared_ptr<SctpTransport> transport,
	            unsigned int stream);
	~DataChannel();

	void close(void);
	void send(const std::variant<binary, string> &data);
	void send(const byte *data, size_t size);
	std::optional<std::variant<binary, string>> receive();

	// Directly send a buffer to avoid a copy
	template <typename Buffer> void sendBuffer(const Buffer &buf);
	template <typename Iterator> void sendBuffer(Iterator first, Iterator last);

	bool isOpen(void) const;
	bool isClosed(void) const;
	size_t availableAmount() const;
	size_t maxMessageSize() const;

	unsigned int stream() const;
	string label() const;
	string protocol() const;
	Reliability reliability() const;

private:
	void open(std::shared_ptr<SctpTransport> sctpTransport);
	void outgoing(mutable_message_ptr message);
	void incoming(message_ptr message);
	void processOpenMessage(message_ptr message);

	const std::shared_ptr<PeerConnection> mPeerConnection;
	std::shared_ptr<SctpTransport> mSctpTransport;

	unsigned int mStream;
	string mLabel;
	string mProtocol;
	std::shared_ptr<Reliability> mReliability;

	std::atomic<bool> mIsOpen = false;
	std::atomic<bool> mIsClosed = false;

	Queue<message_ptr> mRecvQueue;
	std::atomic<size_t> mRecvAmount = 0;

	friend class PeerConnection;
};

template <typename Buffer> std::pair<const byte *, size_t> to_bytes(const Buffer &buf) {
	using T = typename std::remove_pointer<decltype(buf.data())>::type;
	using E = typename std::conditional<std::is_void<T>::value, byte, T>::type;
	return std::make_pair(static_cast<const byte *>(static_cast<const void *>(buf.data())),
	                      buf.size() * sizeof(E));
}

template <typename Buffer> void DataChannel::sendBuffer(const Buffer &buf) {
	auto [bytes, size] = to_bytes(buf);
	auto message = std::make_shared<Message>(size);
	std::copy(bytes, bytes + size, message->data());
	outgoing(message);
}

template <typename Iterator> void DataChannel::sendBuffer(Iterator first, Iterator last) {
	size_t size = 0;
	for (Iterator it = first; it != last; ++it)
		size += it->size();

	auto message = std::make_shared<Message>(size);
	auto pos = message->begin();
	for (Iterator it = first; it != last; ++it) {
		auto [bytes, size] = to_bytes(*it);
		pos = std::copy(bytes, bytes + size, pos);
	}
	outgoing(message);
}

} // namespace rtc

#endif
