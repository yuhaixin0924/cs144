#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>
struct sent_but_unacked
{
 uint64_t begin_abs;// 报文起始的绝对序列号
 TCPSenderMessage message;// seqno、SYN、payload、FIN、RST
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ),current_RTO_ms_(initial_RTO_ms)
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }
  bool syn_sent_{};
  ByteStream input_;
  Wrap32 isn_;//Initial Sequence Number，即本方向的初始 TCP 序列号
  uint64_t initial_RTO_ms_;//Retransmission Timeout，重传超时时间，保存最初设定的超时时长
  std::deque<sent_but_unacked>sent_but_unacked_;
  uint64_t acked_abs_{};
  uint64_t next_abs_{};
  uint64_t window_size_{1};
  bool fin_sent_ {};

  uint64_t elapsed_ms_ {};
  uint64_t current_RTO_ms_ {};
  uint64_t consecutive_retx_ {};
};
