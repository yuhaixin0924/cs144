#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t next_ip=next_hop.ipv4_numeric();
  const auto it = arp_cache_.find(next_ip);
  if(it!=arp_cache_.end()){
    send_ipv4_frame(dgram,it->second.mac);
    return;
  }
    
  waiting_datagrams_[next_ip].push(dgram);
  
  if(arp_request_age_ms_.find(next_ip)!=arp_request_age_ms_.end()){
    return;
  }
  arp_request_age_ms_[next_ip]=0;
  send_arp_request(next_ip);
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if(frame.header.dst!=ethernet_address_&&frame.header.dst!=ETHERNET_BROADCAST){
    return;
  }
  if(frame.header.type==EthernetHeader::TYPE_IPv4){
    InternetDatagram dgram{};
    if(!parse(dgram,frame.payload)){
      return;
    }
    datagrams_received().push(dgram);
  }
  if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp {};
    if (!parse(arp, frame.payload)) {
      return;
    }
    const uint32_t sender_ip=arp.sender_ip_address;
    const EthernetAddress sender_mac=arp.sender_ethernet_address;
    arp_cache_[sender_ip]=ArpEntry {sender_mac, 0 };

    const auto it =waiting_datagrams_.find(sender_ip);
    if(it!=waiting_datagrams_.end()){
      auto &dgrams=it->second;
      while(!dgrams.empty()){
        send_ipv4_frame(dgrams.front(),sender_mac);
        dgrams.pop();
      }
      waiting_datagrams_.erase(it);
    }
    arp_request_age_ms_.erase(sender_ip);
    if(arp.opcode == ARPMessage::OPCODE_REQUEST&&arp.target_ip_address == ip_address_.ipv4_numeric()){
      ARPMessage reply{};
      reply.opcode=ARPMessage::OPCODE_REPLY;
      reply.sender_ip_address=ip_address_.ipv4_numeric();
      reply.sender_ethernet_address=ethernet_address_;
      reply.target_ip_address=sender_ip;
      reply.target_ethernet_address=sender_mac;

      EthernetFrame reply_frame{};
      reply_frame.header.src=ethernet_address_;
      reply_frame.header.dst=sender_mac;
      reply_frame.header.type=EthernetHeader::TYPE_ARP;
      
      reply_frame.payload=serialize(reply);
      transmit(reply_frame);
    }
  }
}
//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{ 
  uint64_t age;
  for (auto it = arp_cache_.begin();it != arp_cache_.end(); ) {
    age=it->second.age_ms+=ms_since_last_tick;
    if(age>=30000){
      waiting_datagrams_.erase(it->first);
      it=arp_cache_.erase(it);
    }
    else{
      it++;
    }
  }

  for (auto it = arp_request_age_ms_.begin();it != arp_request_age_ms_.end(); ) {
    age=it->second+=ms_since_last_tick;
    if(age>=5000){
      waiting_datagrams_.erase(it->first);
      it=arp_request_age_ms_.erase(it);
    }
    else{
      it++;
    }
  }
}
void NetworkInterface::send_ipv4_frame(const InternetDatagram& dgram,const EthernetAddress& destination){
  EthernetFrame frame{};
  frame.header.src=ethernet_address_;
  frame.header.dst=destination;
  frame.header.type=EthernetHeader::TYPE_IPv4;
  frame.payload=serialize(dgram);
  transmit(frame);
}
void NetworkInterface::send_arp_request(uint32_t target_ip){
//构造查询arp
  ARPMessage arp {};
  arp.opcode=ARPMessage::OPCODE_REQUEST;//操作：查询

  arp.sender_ethernet_address=ethernet_address_;//我的MAC
  arp.sender_ip_address=ip_address_.ipv4_numeric();//我的IP

  arp.target_ip_address=target_ip;//下一跳IP
//构造外层以太网帧
  EthernetFrame frame{};
  frame.header.src=ethernet_address_;
  frame.header.dst=ETHERNET_BROADCAST;
  frame.header.type=EthernetHeader::TYPE_ARP;
  
  frame.payload=serialize(arp);
  transmit(frame);
}