#include "ns3/animation-interface.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/random-walk-2d-mobility-model.h"


/*
                   Network Topology

n0 -------------\                   /---------------n2
  (wifi)         r1---------------r2 (wifi)
n1 -------------/   p2p bottleneck  \---------------n3
*/



using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi_simul");

int n_nodes = 1;
int pktsRecvAP = 0, pktsDropAP = 0;

static double
GetAverageDelayOfWiFiNodes(NodeContainer &staNodes, Ptr<Node> apNode)
{
    double avgDelay = 0;
    for(uint32_t i = 0; i < staNodes.GetN(); i++)
    {
        avgDelay += std::sqrt(MobilityHelper::GetDistanceSquaredBetween(staNodes.Get(i), apNode)) * 1000000000.0 / 299792458;
    }
    avgDelay /= staNodes.GetN();
    return avgDelay;
}

static void
PhyRxBeginTrace(Ptr<const Packet> packet, RxPowerWattPerChannelBand rxPowersW)
{
    pktsRecvAP++;
}

static void
PhyRxDropTrace(Ptr<const Packet> packet, ns3::WifiPhyRxfailureReason reason)
{
    pktsDropAP++;
    // std::cout << reason << std::endl;
}

static void
GoodputChange(Ptr<OutputStreamWrapper> file, Ptr<PacketSink> sink1, double prevBytesThrough)
{
    double recvBytes = sink1->GetTotalRx();
    double throughput = ((recvBytes - prevBytesThrough) * 8);
    *file->GetStream() << Simulator::Now().GetSeconds() << "," << throughput << std::endl;
    Simulator::Schedule(MilliSeconds(1.0), &GoodputChange, file, sink1, recvBytes);
}

static void
CwndChange(Ptr<OutputStreamWrapper> file, uint32_t oldval, uint32_t newval)
{
    *file->GetStream() << Simulator::Now().GetSeconds() << "," << newval << '\n';
}

static void
TraceCwnd()
{
    AsciiTraceHelper asciiTraceHelper;
    for (int i = 0; i < n_nodes; i++)
    {
        Ptr<OutputStreamWrapper> file =
            asciiTraceHelper.CreateFileStream("results/wifi_cwnd" + std::to_string(i) + ".csv");
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) +
                                          "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                      MakeBoundCallback(&CwndChange, file));
    }
}

static void
TraceGoodput(ApplicationContainer* apps)
{
    AsciiTraceHelper asciiTraceHelper;
    for (int i = 0; i < n_nodes; i++)
    {
        Ptr<OutputStreamWrapper> file =
            asciiTraceHelper.CreateFileStream("results/wifi_goodput" + std::to_string(i) + ".csv");
        Simulator::Schedule(
            MilliSeconds(1.0),
            MakeBoundCallback(&GoodputChange, file, apps->Get(i)->GetObject<PacketSink>(), 0.0));
    }
}

void
TraceDropRatio()
{
    Config::ConnectWithoutContext(
        "/NodeList/" + std::to_string(n_nodes) +
            "/DeviceList/1/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyRxBegin",
        MakeBoundCallback(&PhyRxBeginTrace));
    Config::ConnectWithoutContext(
        "/NodeList/" + std::to_string(n_nodes) +
            "/DeviceList/1/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyRxDrop",
        MakeBoundCallback(&PhyRxDropTrace));
}

// static void
// DataRateTrace (uint64_t currRate, uint64_t prevrate)
// {
//     std::cout << Simulator::Now().GetSeconds () << "\t Data Rate Now: " << currRate*1.0/(1024*1024) << "Mbps" << std::endl;
// }

// void
// TraceDataRate()
// {
//     Config::ConnectWithoutContext(
//         "/NodeList/" + std::to_string(0) +
//             "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/$ns3::MinstrelHtWifiManager/Rate",
//         MakeBoundCallback(&DataRateTrace));
// }

// double lastTime = 0;
// unsigned long totalMessagesSent = 0;

// static void
// TraceTx(Ptr<const Packet> packet)
// {
//     lastTime = Simulator::Now().GetSeconds();
//     totalMessagesSent++;
// }

int
main(int argc, char* argv[])
{
    std::string phyMode("DsssRate11Mbps");
    std::string tcp_mode = "TcpCubic";
    int runtime = 50; // Seconds
    double rss = -80; // -dBm
    bool enable_log = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numnodes", "number of senders in simulation", n_nodes);
    cmd.AddValue("logging", "turn on all Application log components", enable_log);
    cmd.AddValue("tcpmode", "specify the type of tcp socket", tcp_mode);
    cmd.AddValue("runtime", "Time (in Seconds) sender applications run", runtime);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("rss", "received signal strength", rss);
    cmd.Parse(argc, argv);

    if (enable_log)
    {
        LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    // Fix non-unicast data rate to be the same as that of unicast
    // Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcp_mode));
    // Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(false));
    // Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("10p"));

    PointToPointHelper p2phelper;
    p2phelper.SetChannelAttribute("Delay", StringValue("50ns"));
    p2phelper.SetDeviceAttribute("DataRate", StringValue("11Mbps"));

    PointToPointHelper p2pbottleneckhelper;
    p2pbottleneckhelper.SetChannelAttribute("Delay", StringValue("10ms"));
    p2pbottleneckhelper.SetDeviceAttribute("DataRate", StringValue("1Mbps"));

    NodeContainer leftwifinodes(n_nodes);
    PointToPointDumbbellHelper dumbbellhelper(0,
                                              p2phelper,
                                              n_nodes,
                                              p2phelper,
                                              p2pbottleneckhelper);

    // The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    // wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode)
                                );
    // wifi.EnableLogComponents(); // Turn on all Wifi logging

    YansWifiPhyHelper wifiPhy;
    // This is one parameter that matters when using FixedRssLossModel
    // set it to zero; otherwise, gain will be added
    wifiPhy.Set("RxGain", DoubleValue(0));
    // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    // The below FixedRssLossModel will cause the rss to be fixed regardless
    // of the distance between the two stations, and the transmit power
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
    wifiPhy.SetChannel(wifiChannel.Create());

    // Add a mac and disable rate control
    WifiMacHelper wifiMac;

    // Setup the rest of the MAC
    Ssid ssid = Ssid("wifi-default");
    // setup STA
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, leftwifinodes);
    NetDeviceContainer devices = staDevices;
    // setup AP
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, dumbbellhelper.GetLeft());
    devices.Add(apDevice);

    // Note that with FixedRssLossModel, the positions below are not
    // used for received signal strength.
    MobilityHelper mobility;

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(dumbbellhelper.GetLeft());
    dumbbellhelper.GetLeft()->GetObject<MobilityModel>()->SetPosition(Vector(50, 50, 0));
    
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
        "rho", DoubleValue(20),
        "X", DoubleValue(50),
        "Y", DoubleValue(50)
    );
    mobility.SetMobilityModel (
        "ns3::RandomWalk2dMobilityModel", 
        "Bounds", RectangleValue (Rectangle (0, 100, 0, 100))
        );
    mobility.Install(leftwifinodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(dumbbellhelper.GetRight());
    dumbbellhelper.GetRight()->GetObject<MobilityModel>()->SetPosition(Vector(150, 150, 0));
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
        "rho", DoubleValue(20),
        "X", DoubleValue(150),
        "Y", DoubleValue(150)
    );
    for (int i = 0; i < n_nodes; i++)
    {
        mobility.Install(dumbbellhelper.GetRight(i));
    }

    std::cout << "Average delay of wifi channel = " << GetAverageDelayOfWiFiNodes(leftwifinodes, dumbbellhelper.GetLeft()) << "ns"
              << '\n';

    InternetStackHelper stack;
    dumbbellhelper.InstallStack(stack);
    stack.Install(leftwifinodes);

    Ipv4AddressHelper ipv4left, ipv4bottleneck, ipv4right;
    ipv4left.SetBase("10.1.1.0", "255.255.255.0");
    ipv4bottleneck.SetBase("10.1.2.0", "255.255.255.0");
    ipv4right.SetBase("10.1.3.0", "255.255.255.0");

    Ipv4InterfaceContainer leftwifiinterfaces = ipv4left.Assign(staDevices);
    ipv4left.Assign(apDevice);
    dumbbellhelper.AssignIpv4Addresses(ipv4left, ipv4right, ipv4bottleneck);

    ApplicationContainer senderApps;
    for (int i = 0; i < n_nodes; i++)
    {
        // OnOffHelper onoffhelper("ns3::TcpSocketFactory",
        //                          InetSocketAddress(dumbbellhelper.GetRightIpv4Address(i), 800)
        //                          );
        // onoffhelper.SetConstantRate(DataRate(100*1024*1024));
        // senderApps.Add(onoffhelper.Install(leftwifinodes.Get(i)));

        // senderApps.Get(i)->TraceConnectWithoutContext("Tx", MakeCallback(&TraceTx));

        BulkSendHelper blksender("ns3::TcpSocketFactory",
                                 InetSocketAddress(dumbbellhelper.GetRightIpv4Address(i), 800));
        senderApps.Add(blksender.Install(leftwifinodes.Get(i)));
    }

    ApplicationContainer recvApps;
    for (int i = 0; i < n_nodes; i++)
    {
        PacketSinkHelper pktsink("ns3::TcpSocketFactory",
                                 InetSocketAddress(Ipv4Address::GetAny(), 800));
        recvApps.Add(pktsink.Install(dumbbellhelper.GetRight(i)));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    senderApps.Start(Seconds(0.0));
    recvApps.Start(Seconds(0.0));

    senderApps.Stop(Seconds(runtime));
    recvApps.Stop(Seconds(runtime));

    // Tracing
    wifiPhy.EnablePcap("results/wifi_ap", apDevice);
    wifiPhy.EnablePcap("results/wifi_node", staDevices);
    p2pbottleneckhelper.EnablePcap("results/wifi_router1", dumbbellhelper.GetLeft()->GetDevice(0));
    p2pbottleneckhelper.EnablePcap("results/wifi_router2", dumbbellhelper.GetRight()->GetDevice(0));

    Simulator::Schedule(Seconds(1.001), &TraceCwnd);
    // Simulator::Schedule(Seconds(1.001), &TraceDataRate);
    // Simulator::Schedule(Seconds(1.001), MakeBoundCallback(&TraceGoodput, &recvApps));
    Simulator::Schedule(Seconds(1.001), &TraceDropRatio);

    AnimationInterface anim("../animwifi.xml");
    Simulator::Stop(Seconds(runtime + 1));
    Simulator::Run();

    std::cout << "Ratio of dropped packets on AP: " << pktsDropAP * 1.0 / pktsRecvAP
              << '\n';

    // std::cout << "Sending rate: " << 512*totalMessagesSent*1.0/(1024*1024*lastTime) << "Mbps\n";
    
    for (int i = 0; i < n_nodes; i++)
    {
        std::cout << "Avg. Goodput (Mbps) for flow " + std::to_string(i) + ": "
                  << recvApps.Get(i)->GetObject<PacketSink>()->GetTotalRx() * 8.0 / (runtime * 1024 * 1024) << '\n';
    }

    Simulator::Destroy();

    return 0;
}
