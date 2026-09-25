#include "tcp_receiver.hh"
#include "debug.hh"

#include <algorithm>
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if (!isn_.has_value() ) { 
    if(!message.SYN) return;// 没有零点，也无法从本包获得零点
    isn_ = message.seqno;//本包给出了零点
  }
  //能执行到这里，就一定有 ISN，才能安全调用 isn_.value()
  const uint64_t checkpoint = reassembler_.writer().bytes_pushed() + 1;
  const uint64_t absolute_seqno = message.seqno.unwrap( isn_.value(), checkpoint ); // unwrap(ISN,C)
  if ( absolute_seqno == 0 && !message.SYN )return;
  const uint64_t stream_seqno = absolute_seqno + message.SYN - 1;
  reassembler_.insert( stream_seqno, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage result {};

  // 通告窗口表示 ByteStream 当前还能容纳多少字节。
  // window_size 只有 16 位，因此不能超过 UINT16_MAX。
  const uint64_t available_capacity = writer().available_capacity();
  result.window_size = static_cast<uint16_t>( min<uint64_t>( available_capacity, UINT16_MAX ) );

  // 如果输入字节流已进入错误状态，就在回复中设置 RST。
  result.RST = reader().has_error();

  // 尚未收到 SYN 时不知道 ISN，无法计算 ACK；保留空的 ackno。
  if ( !isn_.has_value() ) {
    return result;
  }

  // SYN 占用一个序列号。bytes_pushed() 是累计写入的连续数据字节数，
  // 即使应用已经读走这些字节，这个计数也不会减少。
  uint64_t next_absolute_seqno = 1 + writer().bytes_pushed();

  // 只有 Reassembler 拼齐 FIN 之前的所有数据并关闭 Writer 后，
  // ACK 才能再前进一位，确认 FIN 占用的序列号。
  if ( writer().is_closed() ) {
    ++next_absolute_seqno;
  }

  result.ackno = Wrap32::wrap( next_absolute_seqno, isn_.value() );
  return result;
}
