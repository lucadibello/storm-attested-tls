package org.apache.storm.messaging.atls;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Supplier;
import org.apache.storm.messaging.IConnection;
import org.apache.storm.messaging.IConnectionCallback;
import org.apache.storm.messaging.IContext;
import org.apache.storm.messaging.netty.NettyRenameThreadFactory;
import org.apache.storm.metrics2.StormMetricRegistry;

public class Context implements IContext {

    private final List<Server> serverConnections = new ArrayList<>();
    private Map<String, Object> topoConf;
    private StormMetricRegistry metricRegistry;

    // Thread pools / schedulers for client/server housekeeping (retries, timers, etc.)
    private ScheduledExecutorService scheduler;
    private ExecutorService ioExecutor;

    @Override
    public void prepare(Map<String, Object> topoConf) {
        prepare(topoConf, null);
    }

    @Override
    public void prepare(Map<String, Object> topoConf, StormMetricRegistry metricRegistry) {
        this.topoConf = topoConf;
        this.metricRegistry = metricRegistry;

        // cached pool for I/O tasks + wheel scheduler for timeouts/backoff.
        this.ioExecutor = Executors.newCachedThreadPool(new NettyRenameThreadFactory("atls-io"));
        this.scheduler = Executors.newSingleThreadScheduledExecutor(new NettyRenameThreadFactory("atls-scheduler"));
    }


    @Override
    public synchronized void term() {
        // Close servers first to stop accepting new traffic.
        for (Server s : serverConnections) {
            s.close();
        }
        serverConnections.clear();

        // Then shut down executors.
        if (scheduler != null) {
            scheduler.shutdownNow();
            try {
                scheduler.awaitTermination(5, TimeUnit.SECONDS);
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
            scheduler = null;
        }
        if (ioExecutor != null) {
            ioExecutor.shutdownNow();
            try {
                ioExecutor.awaitTermination(5, TimeUnit.SECONDS);
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
            ioExecutor = null;
        }
    }

    @Override
    public IConnection bind(String stormId, int port, IConnectionCallback cb, Supplier<Object> newConnectionResponse) {
        // Server owns the listening socket and enclave TLS termination.
        Server server = new Server(topoConf, port, cb, newConnectionResponse, ioExecutor, scheduler, metricRegistry);
        serverConnections.add(server);
        return server;
    }

    @Override
    public IConnection connect(String stormId, String host, int port, AtomicBoolean[] remoteBpStatus) {
        // Client manages a pooled RA-TLS session to (host, port) and batching semantics.
        return new Client(topoConf, host, port, remoteBpStatus, ioExecutor, scheduler, metricRegistry);
    }
}
