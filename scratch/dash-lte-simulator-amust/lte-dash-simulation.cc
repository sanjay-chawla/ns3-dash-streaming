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
 *
 * Author: Sanjay Chawla (schawla@tcd.ie)
 */

#include <iostream>
#include <fstream>

#include "ns3/realtime-simulator-impl.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/internet-module.h"
#include "ns3/node-throughput-tracer.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/AMuSt-module.h"
#include "ns3/dash-http-client-helper.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/buildings-module.h"
#include "ns3/building-position-allocator.h"
#include "ns3/netanim-module.h"

using namespace std;
using namespace ns3;

#include <unistd.h>
#include <iostream>
#include <fstream>


std::string GetCurrentWorkingDir( void ) {
  char buff[250];
  char* cwd = getcwd( buff, 250 );
  std::cout << cwd;
  std::string current_working_dir(buff);
  return current_working_dir;
}

template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}


NS_LOG_COMPONENT_DEFINE ("TapLenaVirtualMachineExample");

int
main (int argc, char *argv[])
{

  LogComponentEnable ("TapLenaVirtualMachineExample", LOG_LEVEL_ALL);
  LogComponentEnable ("DASHFakeServerApplication", LOG_LEVEL_ALL);
  LogComponentEnable ("HttpServerApplication", LOG_LEVEL_ALL);
  LogComponentEnable ("ns3.DASHPlayerTracer", LOG_LEVEL_ALL);
  LogComponentEnable ("MultimediaConsumer", LOG_LEVEL_ALL);
  // LogComponentEnable ("HttpClientApplication", LOG_LEVEL_ALL);

  uint16_t numberOfEnbs = 1;
  double distance = 60.0;
  Time interPacketInterval = MilliSeconds (100);
  bool useCa = false;
  bool disableDl = false;
  bool disableUl = false;
  bool disablePl = true;
  uint32_t simulationId = 1;
  uint32_t numberOfClients = 1;
  double simTime = 10.1;
  std::string DashTraceFile = "report.csv";
  std::string ServerThroughputTraceFile = "server_throughput.csv";
  std::string RepresentationType = "netflix";


  CommandLine cmd;
  cmd.AddValue("numberOfEnbs", "Number of eNBs", numberOfEnbs);
  cmd.AddValue("numberOfClients", "Number of UEs in total", numberOfClients);
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue("interPacketInterval", "Inter packet interval [ms])", interPacketInterval);
  cmd.Parse (argc, argv);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));

  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("100Gbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("5ms"));

  if (useCa)
     {
       Config::SetDefault ("ns3::LteHelper::UseCa", BooleanValue (useCa));
       Config::SetDefault ("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue (2));
       Config::SetDefault ("ns3::LteHelper::EnbComponentCarrierManager", StringValue ("ns3::RrComponentCarrierManager"));
     }
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  cmd.Parse(argc, argv);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  epcHelper->Initialize ();

  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  Names::Add("PGW", pgw);

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  Names::Add("RemoteHost", remoteHost);


  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
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
  enbNodes.Create(numberOfEnbs);
  ueNodes.Create(numberOfClients);

  //////////////////////////////////////////////////////////////////////////////////////////////////
  //// Set up Building
  //////////////////////////////////////////////////////////////////////////////////////////////////
  double roomHeight = 6;
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

  // allocate Enb positions
  for (uint i = 0; i < numberOfEnbs; i++) {
	  positionAlloc->Add(Vector(i*60+30, 0.0, 50.0));
  }
  // allocate clients to positions
  for (uint i = 0; i < numberOfClients; i++)
    {
      // Vector pos = Vector (randPosAlloc->GetNext());
      // positionAlloc->Add (pos);
      positionAlloc->Add(Vector(i*60+30, 5.0, 0.0));
    }

  NS_LOG_DEBUG("before mobility set");
  MobilityHelper staticMobility;
  staticMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  staticMobility.SetPositionAllocator(positionAlloc);
  staticMobility.Install (enbNodes);
  staticMobility.Install (ueNodes);

  BuildingsHelper::Install (enbNodes);
  BuildingsHelper::Install (ueNodes);
  BuildingsHelper::MakeMobilityModelConsistent ();

  NS_LOG_DEBUG("after mobility consistent");


  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (ueLteDevs);
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }


  lteHelper->Attach(ueLteDevs);

  // Install and start applications on UEs and remote host
  uint16_t port = 80;
  uint16_t dlPort = 1100;
  uint16_t ulPort = 2000;
  uint16_t otherPort = 3000;
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  /* Install TCP Receiver on the access point */

  std::string representationStrings = GetCurrentWorkingDir() + "/../content/segments/BigBuckBunny/bunny_2s/dash_dataset_avc_bbb.csv";
  fprintf(stderr, "representations = %s\n", representationStrings.c_str());
  DASHServerHelper server(Ipv4Address::GetAny(), port,  "10.0.0.2", "/content/segments/BigBuckBunny/bunny_2s/", representationStrings, "/content/segments/BigBuckBunny/bunny_2s/");
  serverApps = server.Install (remoteHost);
  serverApps.Start (Seconds (1.0));

  int screenWidth = 1240;
  int screenHeight = 1080;
  std::string AdaptationLogicToUse = "RateBasedAdaptationLogic"; // DASHJSAdaptationLogic
  AdaptationLogicToUse = "dash::player::" + AdaptationLogicToUse;
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

            /*
			 HttpClientHelper client(remoteHostAddr, port,
							"content/segments/BigBuckBunny/bunny_2s/vid1.mpd.gz", "localhost");
			clientApps.Add(client.Install(ueNodes.Get(u)));
			 */
			// installing client
			std::stringstream ssMPDURL;
			ssMPDURL << "http://" << remoteHostAddr << "/content/segments/BigBuckBunny/bunny_2s/vid1.mpd.gz";
			// NS_LOG_DEBUG(">>>>>>>>>>> add: " << ssMPDURL.str());
			DASHHttpClientHelper client(ssMPDURL.str());
			client.SetAttribute("AdaptationLogic", StringValue(AdaptationLogicToUse));
			client.SetAttribute("StartUpDelay", StringValue("0.5"));
			client.SetAttribute("ScreenWidth", UintegerValue(screenWidth));
			client.SetAttribute("ScreenHeight", UintegerValue(screenHeight));
			client.SetAttribute("AllowDownscale", BooleanValue(true));
			client.SetAttribute("AllowUpscale", BooleanValue(true));
			client.SetAttribute("MaxBufferedSeconds", StringValue("1600"));

			clientApps.Add(client.Install(ueNodes.Get(u)));

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

        if (!disablePl && numberOfEnbs > 1)
          {
            ++otherPort;
            PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), otherPort));
            serverApps.Add (packetSinkHelper.Install (ueNodes.Get (u)));

            UdpClientHelper client (ueIpIface.GetAddress (u), otherPort);
            client.SetAttribute ("Interval", TimeValue (interPacketInterval));
            client.SetAttribute ("MaxPackets", UintegerValue (1000000));
            clientApps.Add (client.Install (ueNodes.Get ((u + 1) % numberOfEnbs)));
          }
      }
  clientApps.Start (Seconds (2));
  clientApps.Stop (Seconds (8));


  fprintf(stderr, "Installing DASH Tracers on all clients\n");
  DASHPlayerTracer::Install(ueNodes, DashTraceFile);

  fprintf(stderr, "Installing one NodeThroughputTracer\n");
  NodeThroughputTracer::Install(remoteHost, ServerThroughputTraceFile);

  lteHelper->EnableTraces ();
  p2ph.EnablePcapAll("lena-simple-epc-p2p-", true);
  // NS_LOG_UNCOND("Installing Routing Tables");
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  AsciiTraceHelper ascii;
  p2ph.EnableAsciiAll (ascii.CreateFileStream ("lte-dash-streaming.tr"));
  Ipv4RoutingHelper::PrintRoutingTableEvery(Seconds (5), remoteHost, ascii.CreateFileStream ("hostRoutingTable.txt"), Time::S);

  //
  // Run the simulation for ten minutes to give the user time to play around
  //
  Simulator::Stop (Seconds (10.));
  ns3::AnimationInterface *anim;
  anim = new AnimationInterface ("animation.xml");
  anim->AddResource("");
  Simulator::Run ();
  Simulator::Destroy ();
}
