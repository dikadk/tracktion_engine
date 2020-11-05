/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#ifdef _MSC_VER
 #pragma warning (push)
 #pragma warning (disable: 4127)
#endif

#include "../3rd_party/concurrentqueue.h"

#ifdef _MSC_VER
 #pragma warning (pop)
#endif

namespace tracktion_graph
{

/**
    Plays back a node with mutiple threads.
    The setting of Nodes and processing are all lock-free.
    This means the player can be real-time safe as long as your provided ThreadPool
    doesn't do any non-real-time operations in the wait/signal methods.
*/
class LockFreeMultiThreadedNodePlayer
{
public:
    //==============================================================================
    /**
        Base class for thread pools which can be customised to determine how
        cooperative threads should behave.
    */
    struct ThreadPool
    {
        /** Constructs a ThreadPool for a given LockFreeMultiThreadedNodePlayer. */
        ThreadPool (LockFreeMultiThreadedNodePlayer& p)
            : player (p)
        {
        }
        
        /** Destructor. */
        virtual ~ThreadPool() = default;
        
        /** Subclasses should implement this to create the given number of threads. */
        virtual void createThreads (size_t numThreads) = 0;
        
        /** Subclasses should implement this to clear all the threads. */
        virtual void clearThreads() = 0;

        /** Called by the player when a Node becomes available to process.
            Subclasses should use this to try and get a thread to call process as soon as possible.
        */
        virtual void signalOne() = 0;

        /** Called by the player when more than one Node becomes available to process.
            Subclasses should use this to try and get a thread to call process as soon as possible.
        */
        virtual void signalAll() = 0;
        
        /** Called by the player when the audio thread has no free Nodes to process.
            Subclasses should can use this to either spin, pause or wait until a Node does
            become free or isFinalNodeReady returns true.
        */
        virtual void waitForFinalNode() = 0;

        //==============================================================================
        /** Signals the pool that all the threads should exit. */
        void signalShouldExit()
        {
            threadsShouldExit = true;
            signalAll();
        }
        
        /** Signals the pool that all the threads should continue to run and not exit. */
        void resetExitSignal()
        {
            threadsShouldExit = false;
        }
        
        /** Returns true if all the threads should exit. */
        bool shouldExit() const
        {
            return threadsShouldExit.load (std::memory_order_acquire);
        }

        /** Returns true if there are no free Nodes to be processed and the calling
            thread should wait until there are Nodes ready.
        */
        bool shouldWait()
        {
            if (shouldExit())
                return false;
            
            return player.numNodesQueued == 0;
        }

        /** Returns true if all the Nodes have been processed. */
        bool isFinalNodeReady()
        {
            if (! player.preparedNode.rootNode)
                return true;

            return player.preparedNode.rootNode->hasProcessed();
        }

        /** Process the next chain of Nodes.
            Returns true if at least one Node was processed, false if no Nodes were processed.
            You can use this to determine how long to pause/wait for before processing again.
        */
        bool process()
        {
            return player.processNextFreeNode();
        }
        
    private:
        LockFreeMultiThreadedNodePlayer& player;
        std::atomic<bool> threadsShouldExit { false };
    };

    //==============================================================================
    /** Creates an empty LockFreeMultiThreadedNodePlayer. */
    LockFreeMultiThreadedNodePlayer();
    
    using ThreadPoolCreator = std::function<std::unique_ptr<ThreadPool> (LockFreeMultiThreadedNodePlayer&)>;
    
    /** Creates an empty LockFreeMultiThreadedNodePlayer with a specified ThreadPool type. */
    LockFreeMultiThreadedNodePlayer (ThreadPoolCreator);
    
    /** Destructor. */
    ~LockFreeMultiThreadedNodePlayer();
    
    //==============================================================================
    /** Sets the number of threads to use for rendering.
        This can be 0 in which case only the process calling thread will be used for processing.
        N.B. this will pause processing whilst updating the threads so there will be a gap in the audio.
    */
    void setNumThreads (size_t);
    
    /** Sets the Node to process. */
    void setNode (std::unique_ptr<Node>);

    /** Sets the Node to process with a new sample rate and block size. */
    void setNode (std::unique_ptr<Node> newNode, double sampleRateToUse, int blockSizeToUse);

    /** Prepares the current Node to be played. */
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse, Node* oldNode = nullptr);

    /** Returns the current Node. */
    Node* getNode()
    {
        return rootNode;
    }

    /** Process a block of the Node. */
    int process (const Node::ProcessContext&);
    
    /** Returns the current sample rate. */
    double getSampleRate() const
    {
        return sampleRate.load (std::memory_order_acquire);
    }

private:
    //==============================================================================
    std::atomic<size_t> numThreadsToUse { std::max ((size_t) 0, (size_t) std::thread::hardware_concurrency() - 1) };
    juce::Range<int64_t> referenceSampleRange;
    std::atomic<bool> threadsShouldExit { false };

    std::unique_ptr<ThreadPool> threadPool;
    
    struct PlaybackNode
    {
        PlaybackNode (Node& n)
            : node (n), numInputs (node.getDirectInputNodes().size())
        {}
        
        Node& node;
        const size_t numInputs;
        std::vector<Node*> outputs;
        std::atomic<size_t> numInputsToBeProcessed { 0 };
        std::atomic<bool> hasBeenQueued { true };
       #if JUCE_DEBUG
        std::atomic<bool> hasBeenDequeued { false };
       #endif
    };
    
    struct PreparedNode
    {
        std::unique_ptr<Node> rootNode;
        std::vector<Node*> allNodes;
        std::vector<std::unique_ptr<PlaybackNode>> playbackNodes;
        moodycamel::ConcurrentQueue<Node*> nodesReadyToBeProcessed;
    };
    
    Node* rootNode = nullptr;
    PreparedNode preparedNode, pendingPreparedNodeStorage;
    std::atomic<PreparedNode*> pendingPreparedNode { nullptr };
    std::atomic<bool> isUpdatingPreparedNode { false };
    std::atomic<size_t> numNodesQueued { 0 };

    //==============================================================================
    std::atomic<double> sampleRate { 44100.0 };
    int blockSize = 512;
    
    //==============================================================================
    /** Prepares a specific Node to be played and returns all the Nodes. */
    std::vector<Node*> prepareToPlay (Node* node, Node* oldNode, double sampleRateToUse, int blockSizeToUse);

    //==============================================================================
    void updatePreparedNode();

    //==============================================================================
    void clearThreads();
    void createThreads();
    void pause();

    //==============================================================================
    void setNewCurrentNode (std::unique_ptr<Node> newRoot, std::vector<Node*> newNodes);
    
    //==============================================================================
    static void buildNodesOutputLists (PreparedNode&);
    void resetProcessQueue();
    Node* updateProcessQueueForNode (Node&);
    void processNode (Node&);

    //==============================================================================
    bool processNextFreeNode();
};

}
