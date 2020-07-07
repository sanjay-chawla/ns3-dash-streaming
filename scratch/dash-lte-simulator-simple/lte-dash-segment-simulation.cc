/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
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
 *
 * Author: Sanjay Chawla <schawla@tcd.ie>
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include <ns3/buildings-module.h>
#include "ns3/building-position-allocator.h"

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/applications-module.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"
//#include "ns3/gtk-config-store.h"


template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeBs,
 * attaches one UE per eNodeB starts a flow for each UE to and from a remote host.
 * It also starts another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("LenaSimpleEpc");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("LenaSimpleEpc", LOG_LEVEL_INFO);
  LogComponentEnable ("TcpStreamClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("TcpStreamServerApplication", LOG_LEVEL_INFO);
  uint16_t numNodePairs = 2;
  double distance = 60.0;
  Time interPacketInterval = MilliSeconds (100);
  bool useCa = false;
  bool disableDl = false;
  bool disableUl = false;
  bool disablePl = false;
  
  uint64_t segmentDuration = 2000000;
  // The simulation id is used to distinguish log file results from potentially multiple consequent simulation runs.
  uint32_t simulationId = 1;
  uint32_t numberOfClients = 3;
  std::string adaptationAlgo = "panda";
  std::string segmentSizeFilePath = "src/dash/segmentSizes.txt";

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue ("numNodePairs", "Number of eNodeBs + UE pairs", numNodePairs);
  // cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
  cmd.AddValue ("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
  cmd.AddValue ("useCa", "Whether to use carrier aggregation.", useCa);
  cmd.AddValue ("disableDl", "Disable downlink data flows", disableDl);
  cmd.AddValue ("disableUl", "Disable uplink data flows", disableUl);
  cmd.AddValue ("disablePl", "Disable data flows between peer UEs", disablePl);
  cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
  cmd.AddValue ("numberOfClients", "The number of clients", numberOfClients);
  cmd.AddValue ("segmentDuration", "The duration of a video segment in microseconds", segmentDuration);
  cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
  cmd.AddValue ("segmentSizeFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes", segmentSizeFilePath);

  cmd.Parse (argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line
  // cmd.Parse(argc, argv);

  // No of segments * (segment transport time + segment interval)
  Time simTime = MilliSeconds (221 * 2000 + 144600);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));

  if (useCa)
   {
     Config::SetDefault ("ns3::LteHelper::UseCa", BooleanValue (useCa));
     Config::SetDefault ("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue (2));
     Config::SetDefault ("ns3::LteHelper::EnbComponentCarrierManager", StringValue ("ns3::RrComponentCarrierManager"));
   }

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  NodeContainer allNodes;
  enbNodes.Create (numNodePairs);
  allNodes.Add(enbNodes);
  ueNodes.Create (numberOfClients);
  allNodes.Add(ueNodes);

  /* Determin client nodes for object creation with client helper class */
  std::vector <std::pair <Ptr<Node>, std::string> > clients;
  for (NodeContainer::Iterator i = ueNodes.Begin () + 2; i != ueNodes.End (); ++i)
  	  {
	  	  std::pair <Ptr<Node>, std::string> client (*i, adaptationAlgo);
	  	  clients.push_back (client);
  	  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  //// Set up Building
  //////////////////////////////////////////////////////////////////////////////////////////////////
    double roomHeight = 3;
    double roomLength = 6;
    double roomWidth = 5;
    uint32_t xRooms = 8;
    uint32_t yRooms = 3;
    uint32_t nFloors = 6;

    Ptr<Building> b = CreateObject <Building> ();
    b->SetBoundaries (Box ( 0.0, xRooms * roomWidth,
                            10.0, yRooms * roomLength,
                            0.0, nFloors * roomHeight));
    b->SetBuildingType (Building::Office);
    b->SetExtWallsType (Building::ConcreteWithWindows);
    b->SetNFloors (6);
    b->SetNRoomsX (8);
    b->SetNRoomsY (3);

    Ptr<Building> b2 = CreateObject <Building> ();
	b2->SetBoundaries (Box ( 50.0, xRooms * roomWidth,
							10.0, yRooms * roomLength,
							0.0, nFloors * roomHeight));
	b2->SetBuildingType (Building::Office);
	b2->SetExtWallsType (Building::ConcreteWithWindows);
	b2->SetNFloors (7);
	b2->SetNRoomsX (6);
	b2->SetNRoomsY (4);

    // Install Mobility Model
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

    Ptr<RandomBuildingPositionAllocator> randPosAlloc = CreateObject<RandomBuildingPositionAllocator> ();
    randPosAlloc->AssignStreams (simulationId);

  // create folder so we can log the positions of the clients
    const char * mylogsDir = dashLogDirectory.c_str();
    mkdir (mylogsDir, 0775);
    std::string algodirstr (dashLogDirectory +  adaptationAlgo );  
    const char * algodir = algodirstr.c_str();
    mkdir (algodir, 0775);
    std::string dirstr (dashLogDirectory + adaptationAlgo + "/" + ToString (numberOfClients) + "/");
    const char * dir = dirstr.c_str();
    mkdir(dir, 0775);
    
    NS_LOG_DEBUG("before enB position");

    std::ofstream clientPosLog;
    std::string clientPos = dashLogDirectory + "/" + adaptationAlgo + "/" + ToString (numberOfClients) + "/" + "sim" + ToString (simulationId) + "_"  + "clientPos.txt";
    clientPosLog.open (clientPos.c_str());
    NS_ASSERT_MSG (clientPosLog.is_open(), "Couldn't open clientPosLog file");



//      std::vector< Ptr<ConstantPositionMobilityModel>> mobilityEnb;
//      for (uint16_t i = 0; i < numNodePairs; i++)
//          {
//    	  	// positionAlloc->Add (Vector (distance * i, 0, 0));
//    	    Ptr<ConstantPositionMobilityModel> mm = enbNodes.Get (i)->GetObject<ConstantPositionMobilityModel> ();
//    	    NS_LOG_DEBUG(i);
//    	    mm->SetPosition(Vector (5.0 * i, 5.0, 5.0));
//    	    NS_LOG_DEBUG(i);
//    	    mobilityEnb.push_back(mm);
//    	    NS_LOG_DEBUG(i);
//            // Ptr<ConstantPositionMobilityModel> mm1 = enbNodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
//            // mm1->SetPosition (Vector (30.0, 40.0, 1.5));
//          }

//    Ptr<ConstantPositionMobilityModel> mm0 = enbNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ();
//    Ptr<ConstantPositionMobilityModel> mm1 = enbNodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
//    NS_LOG_DEBUG("1");
//    mm0->SetPosition (Vector (1.0, 1.0, 1.0));
//    NS_LOG_DEBUG("2");
//    mm1->SetPosition (Vector (30.0, 40.0, 1.5));

    Vector posAp = Vector ( 1.0, 1.0, 1.0);
      // give the server node any position, it does not have influence on the simulation, it has to be set though,
      // because when we do: mobility.Install (networkNodes);, there has to be a position as place holder for the server
      // because otherwise the first client would not get assigned the desired position.
      Vector posServer = Vector (45.0, 0.0, 1.5);

      /* Set up positions of nodes (AP and server) */
      positionAlloc->Add (posAp);
      positionAlloc->Add (posServer);

      NS_LOG_DEBUG("before ue position");
      // allocate clients to positions
      for (uint i = 0; i < numberOfClients; i++)
        {
          Vector pos = Vector (randPosAlloc->GetNext());
          positionAlloc->Add (pos);

          // log client positions
          clientPosLog << ToString(pos.x) << ", " << ToString(pos.y) << ", " << ToString(pos.z) << "\n";
          clientPosLog.flush ();
        }

      NS_LOG_DEBUG("before mobility set");
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(allNodes);
  //mobility.Install(ueNodes);

  BuildingsHelper::Install (allNodes); // networkNodes contains all nodes, stations and ap
  BuildingsHelper::MakeMobilityModelConsistent ();

  NS_LOG_DEBUG("after mobility consistent");

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Attach one UE per eNodeB
  /*for (uint16_t i = 0; i < numberOfClients; i++)
    {
      lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
      // side effect: the default EPS bearer will be activated
    }
   */
  lteHelper->Attach(ueLteDevs);

  // Install and start applications on UEs and remote host
  uint16_t port = 9;
  uint16_t dlPort = 1100;
  uint16_t ulPort = 2000;
  uint16_t otherPort = 3000;
  ApplicationContainer serverApps;
//  ApplicationContainer clientApps;

  /* Install TCP Receiver on the access point */
   TcpStreamServerHelper serverHelper (port);
   ApplicationContainer serverApp = serverHelper.Install (remoteHost);
   serverApp.Start (Seconds (1.0));

  TcpStreamClientHelper clientHelper (remoteHostAddr, port);
  clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
  clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
  clientHelper.SetAttribute ("NumberOfClients", UintegerValue(numberOfClients));
  clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));
  ApplicationContainer clientApps = clientHelper.Install (clients);
  for (uint i = 0; i < clientApps.GetN (); i++)
      {
        double startTime = 2.0 + ((i * 3) / 100.0);
        clientApps.Get (i)->SetStartTime (Seconds (startTime));
      }

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      if (!disableDl)
        {
          PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
          serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get (u)));

          UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
          dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
          clientApps.Add (dlClient.Install (remoteHost));
        }

      if (!disableUl)
        {
          ++ulPort;
          PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
          serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

          UdpClientHelper ulClient (remoteHostAddr, ulPort);
          ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
          clientApps.Add (ulClient.Install (ueNodes.Get(u)));
        }

      if (!disablePl && numNodePairs > 1)
        {
          ++otherPort;
          PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), otherPort));
          serverApps.Add (packetSinkHelper.Install (ueNodes.Get (u)));

          UdpClientHelper client (ueIpIface.GetAddress (u), otherPort);
          client.SetAttribute ("Interval", TimeValue (interPacketInterval));
          client.SetAttribute ("MaxPackets", UintegerValue (1000000));
          clientApps.Add (client.Install (ueNodes.Get ((u + 1) % numNodePairs)));
        }
    }

  serverApps.Start (MilliSeconds (500));
  clientApps.Start (MilliSeconds (500));
  lteHelper->EnableTraces ();
  // Uncomment to enable PCAP tracing
  p2ph.EnablePcapAll("lena-simple-epc-", true);

  Simulator::Stop (simTime);
  Simulator::Run ();

  /*GtkConfigStore config;
  config.ConfigureAttributes();*/

  Simulator::Destroy ();
  return 0;
}
