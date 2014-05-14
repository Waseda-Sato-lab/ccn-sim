/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

// Default Network Topology(second.cc from tutorial)
//
//       10.1.1.0
// n0 -------------- n1   n2   n3   n4
//    point-to-point  |    |    |    |
//                    ================
//                      LAN 10.1.2.0

/////ZHU LI: Only CSMA NODES!!!/////
//
//      
//     n1   n2   n3   n4   n5   n6(server)
//     |    |    |    |    |    |
//     ===========================
//            LAN 10.1.1.0


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CSMA-BUS");//Default: SecondScriptExample

int 
main (int argc, char *argv[])
{
  uint32_t nCsma = 5;
  //uint32_t maxBytes = 32000;

  CommandLine cmd;
  //cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
  //cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);

  cmd.Parse (argc,argv);

  // CSMA Channel setting
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("800Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (4)));
  
  // Create csmaNodes(csmaServer & csmaClient)
  NS_LOG_INFO ("Create nodes.");
  NodeContainer csmaServer;
  csmaServer.Create(1);

  NodeContainer csmaClient;
  csmaClient.Create(nCsma);

  NodeContainer global = NodeContainer::GetGlobal ();
  uint32_t size = global.GetN ();
  
  NodeContainer csmaNodes = NodeContainer(csmaServer, csmaClient);

  //Install CSMA Devices
  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  // Install network stacks on the nodes
  InternetStackHelper stack;
  stack.Install (csmaNodes);

  //Add IP Addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);
  
  //Global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create a packet sink on csmaServer
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkhelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkhelper.Install (csmaServer);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));


  // Create the OnOff applications to send TCP to the server
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  //clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  //clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute("PacketSize", UintegerValue (32000));
  clientHelper.SetAttribute("DataRate", StringValue ("100Mbps"));

  ApplicationContainer clientApp;

   for (uint32_t i = 0; i < size-1; ++i)
     {
       AddressValue remoteAddress (sinkLocalAddress);
       clientHelper.SetAttribute ("Remote", remoteAddress);
       clientApp.Add (clientHelper.Install (csmaClient.Get (i)));
     }
   clientApp.Start (Seconds (1.0));
   clientApp.Stop (Seconds (10.0));


  //Configure tracing
  //pointToPoint.EnablePcapAll ("csma-bus");
  csma.EnablePcap ("csma-bus", csmaDevices.Get(0), true);


  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
