#ifndef LLARP_SERVICE_ENDPOINT_HPP
#define LLARP_SERVICE_ENDPOINT_HPP
#include <llarp/codel.hpp>
#include <llarp/pathbuilder.hpp>
#include <llarp/service/Identity.hpp>
#include <llarp/service/handler.hpp>
#include <llarp/service/protocol.hpp>

namespace llarp
{
  namespace service
  {
    struct Endpoint : public llarp_pathbuilder_context,
                      public ILookupHolder,
                      public IDataHandler
    {
      /// minimum interval for publishing introsets
      static const llarp_time_t INTROSET_PUBLISH_INTERVAL =
          DEFAULT_PATH_LIFETIME / 4;

      static const llarp_time_t INTROSET_PUBLISH_RETRY_INTERVAL = 5000;

      Endpoint(const std::string& nickname, llarp_router* r);
      ~Endpoint();

      void
      SetHandler(IDataHandler* h);

      bool
      SetOption(const std::string& k, const std::string& v);

      void
      Tick(llarp_time_t now);

      /// router's logic
      llarp_logic*
      RouterLogic();

      /// endpoint's logic
      llarp_logic*
      EndpointLogic();

      llarp_crypto*
      Crypto();

      llarp_threadpool*
      Worker();

      llarp_router*
      Router()
      {
        return m_Router;
      }

      bool
      Start();

      std::string
      Name() const;

      bool
      ShouldPublishDescriptors(llarp_time_t now) const;

      bool
      PublishIntroSet(llarp_router* r);

      bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg);

      bool
      HandleGotRouterMessage(const llarp::dht::GotRouterMessage* msg);

      bool
      HandleHiddenServiceFrame(const llarp::service::ProtocolFrame* msg);

      /// return true if we have an established path to a hidden service
      bool
      HasPathToService(const Address& remote) const;

      /// return true if we have a pending job to build to a hidden service but
      /// it's not done yet
      bool
      HasPendingPathToService(const Address& remote) const;

      /// return false if we don't have a path to the service
      /// return true if we did and we removed it
      bool
      ForgetPathToService(const Address& remote);

      virtual void
      HandleDataMessage(ProtocolMessage* msg)
      {
        // override me in subclass
      }

      /// ensure that we know a router, looks up if it doesn't
      void
      EnsureRouterIsKnown(const RouterID& router);

      Identity*
      GetIdentity()
      {
        return &m_Identity;
      }

      void
      PutLookup(IServiceLookup* lookup, uint64_t txid);

      void
      HandlePathBuilt(path::Path* path);

      /// context needed to initiate an outbound hidden service session
      struct OutboundContext : public llarp_pathbuilder_context
      {
        OutboundContext(const IntroSet& introSet, Endpoint* parent);
        ~OutboundContext();

        /// the remote hidden service's curren intro set
        IntroSet currentIntroSet;
        /// the current selected intro
        Introduction selectedIntro;

        /// update the current selected intro to be a new best introduction
        void
        ShiftIntroduction();

        /// tick internal state
        /// return true to remove otherwise don't remove
        bool
        Tick(llarp_time_t now);

        /// encrypt asynchronously and send to remote endpoint from us
        void
        AsyncEncryptAndSendTo(llarp_buffer_t D, ProtocolType protocol);

        /// issues a lookup to find the current intro set of the remote service
        void
        UpdateIntroSet();

        void
        HandlePathBuilt(path::Path* path);

        bool
        SelectHop(llarp_nodedb* db, llarp_rc* prev, llarp_rc* cur, size_t hop);

        bool
        HandleHiddenServiceFrame(const ProtocolFrame* frame);

        void
        PutLookup(IServiceLookup* lookup, uint64_t txid);

        std::string
        Name() const;

       private:
        bool
        OnIntroSetUpdate(const IntroSet* i);

        void
        EncryptAndSendTo(llarp_buffer_t payload);

        void
        AsyncGenIntro(llarp_buffer_t payload);

        /// send a fully encrypted hidden service frame
        void
        Send(ProtocolFrame& f);

        uint64_t sequenceNo = 0;
        llarp::SharedSecret sharedKey;
        Endpoint* m_Parent;
        uint64_t m_UpdateIntrosetTX = 0;
      };

      // passed a sendto context when we have a path established otherwise
      // nullptr if the path was not made before the timeout
      typedef std::function< void(OutboundContext*) > PathEnsureHook;

      /// return false if we have already called this function before for this
      /// address
      bool
      EnsurePathToService(const Address& remote, PathEnsureHook h,
                          uint64_t timeoutMS);

      virtual bool
      HandleAuthenticatedDataFrom(const Address& remote, llarp_buffer_t data)
      {
        /// TODO: imlement me
        return true;
      }

      void
      PutSenderFor(const ConvoTag& tag, const ServiceInfo& info);

      bool
      GetCachedSessionKeyFor(const ConvoTag& remote,
                             SharedSecret& secret) const;
      void
      PutCachedSessionKeyFor(const ConvoTag& remote,
                             const SharedSecret& secret);

      bool
      GetSenderFor(const ConvoTag& remote, ServiceInfo& si) const;

      void
      PutIntroFor(const ConvoTag& remote, const Introduction& intro);

      bool
      GetIntroFor(const ConvoTag& remote, Introduction& intro) const;

      bool
      GetConvoTagsForService(const ServiceInfo& si,
                             std::set< ConvoTag >& tag) const;

      void
      PutNewOutboundContext(const IntroSet& introset);

     protected:
      virtual void
      IntroSetPublishFail();
      virtual void
      IntroSetPublished();

      IServiceLookup*
      GenerateLookupByTag(const Tag& tag);

      void
      PrefetchServicesByTag(const Tag& tag);

      uint64_t
      GetSeqNoForConvo(const ConvoTag& tag);

      bool
      IsolateNetwork();

     private:
      bool
      OnOutboundLookup(const IntroSet* i); /*  */

      static bool
      SetupIsolatedNetwork(void* user);

      bool
      DoNetworkIsolation();

      uint64_t
      GenTXID();

     protected:
      IDataHandler* m_DataHandler = nullptr;
      Identity m_Identity;

     private:
      llarp_router* m_Router;
      llarp_threadpool* m_IsolatedWorker = nullptr;
      llarp_logic* m_IsolatedLogic       = nullptr;
      std::string m_Keyfile;
      std::string m_Name;
      std::string m_NetNS;

      std::unordered_map< Address, OutboundContext*, Address::Hash >
          m_RemoteSessions;
      std::unordered_map< Address, PathEnsureHook, Address::Hash >
          m_PendingServiceLookups;

      std::unordered_map< RouterID, uint64_t, RouterID::Hash > m_PendingRouters;

      uint64_t m_CurrentPublishTX       = 0;
      llarp_time_t m_LastPublish        = 0;
      llarp_time_t m_LastPublishAttempt = 0;
      /// our introset
      service::IntroSet m_IntroSet;
      /// pending remote service lookups by id
      std::unordered_map< uint64_t, service::IServiceLookup* > m_PendingLookups;
      /// prefetch remote address list
      std::set< Address > m_PrefetchAddrs;
      /// hidden service tag
      Tag m_Tag;
      /// prefetch descriptors for these hidden service tags
      std::set< Tag > m_PrefetchTags;
      /// on initialize functions
      std::list< std::function< bool(void) > > m_OnInit;

      struct Session
      {
        SharedSecret sharedKey;
        ServiceInfo remote;
        Introduction intro;
        llarp_time_t lastUsed = 0;
        uint64_t seqno        = 0;
      };

      /// sessions
      std::unordered_map< ConvoTag, Session, ConvoTag::Hash > m_Sessions;

      struct CachedTagResult : public IServiceLookup
      {
        const static llarp_time_t TTL = 10000;
        llarp_time_t lastRequest      = 0;
        llarp_time_t lastModified     = 0;
        std::set< IntroSet > result;
        Tag tag;

        CachedTagResult(Endpoint* p, const Tag& t, uint64_t tx)
            : IServiceLookup(p, tx), tag(t)
        {
        }

        ~CachedTagResult();

        void
        Expire(llarp_time_t now);

        bool
        ShouldRefresh(llarp_time_t now) const
        {
          if(now <= lastRequest)
            return false;
          return (now - lastRequest) > TTL;
        }

        llarp::routing::IMessage*
        BuildRequestMessage();

        bool
        HandleResponse(const std::set< IntroSet >& results);
      };

      std::unordered_map< Tag, CachedTagResult, Tag::Hash > m_PrefetchedTags;
    };
  }  // namespace service
}  // namespace llarp

#endif