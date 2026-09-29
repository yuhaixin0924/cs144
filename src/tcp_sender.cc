#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return next_abs_-acked_abs_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retx_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t effective_window = max<uint64_t>( window_size_, 1 );
  while ( true ) {
    if ( next_abs_ - acked_abs_ >= effective_window ) { // 避免无符号下溢
      break;
    }
    uint64_t available_space = effective_window - ( next_abs_ - acked_abs_ ); // 本轮可以占用多少个序列号
    TCPSenderMessage message = make_empty_message();
    if ( !syn_sent_ && available_space ) {
      message.SYN = 1;
      syn_sent_ = 1;
      available_space--;
    }

    uint64_t payload_size = min<uint64_t>( available_space, TCPConfig::MAX_PAYLOAD_SIZE );
    read( reader(), payload_size, message.payload );
    available_space -= message.payload.size();
    if ( reader().is_finished() && !fin_sent_ && available_space > 0 ) {
      message.FIN = true;
      fin_sent_ = true;
      available_space--;
    }
    if ( message.sequence_length() == 0 ) {
      break;
    }
    const bool timer_was_stopped = sent_but_unacked_.empty();
    sent_but_unacked_.push_back( { next_abs_, message } );
    next_abs_ += message.sequence_length();

    transmit( message );

    if ( timer_was_stopped ) {
      elapsed_ms_ = 0;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage message {};
  message.seqno = Wrap32::wrap( next_abs_, isn_ );
  message.RST = input_.has_error();
  return message;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    input_.set_error();
  }

  window_size_ = msg.window_size;

  if ( !msg.ackno.has_value() ) {
    return;
  }
  const uint64_t ack_abs = msg.ackno.value().unwrap( isn_, next_abs_ );

  // 超过 next_abs_：不可能的 ACK
  if ( ack_abs > next_abs_ ) {
    return;
  }

  // 小于或等于当前确认边界：旧 ACK 或重复 ACK
  if ( ack_abs <= acked_abs_ ) {
    return;
  }

  // 到这里说明 ACK 确认了新内容
  acked_abs_ = ack_abs;

  //删除已被完整确认的队首报文
  while(!sent_but_unacked_.empty()){
    uint64_t end_abs=sent_but_unacked_.front().begin_abs+sent_but_unacked_.front().message.sequence_length();
    if(end_abs<=acked_abs_){
    sent_but_unacked_.pop_front();
    }
    else{
      break;
    }
  }
  // 新 ACK 到来，重置重传状态
  elapsed_ms_ = 0;
  current_RTO_ms_ = initial_RTO_ms_;
  consecutive_retx_ = 0;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if(sent_but_unacked_.empty()){
    return;
  }
  elapsed_ms_+=ms_since_last_tick;
  if(elapsed_ms_<current_RTO_ms_){
    return;
  }
  transmit(sent_but_unacked_.front().message);
  if(window_size_>0){
    consecutive_retx_++;
    current_RTO_ms_*=2;
  }
  elapsed_ms_=0;
}
