package org.apache.storm.messaging.atls;

import java.util.Collection;
import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.atomic.AtomicBoolean;

import org.apache.storm.grouping.Load;
import org.apache.storm.messaging.ConnectionWithStatus;
import org.apache.storm.messaging.TaskMessage;
import org.apache.storm.messaging.netty.BackPressureStatus;
import org.apache.storm.metrics2.StormMetricRegistry;


public class Client extends ConnectionWithStatus {

    Client(Map<String, Object> conf, String host, int port, AtomicBoolean[] remoteBpStatus,
           ExecutorService io, ScheduledExecutorService sched, StormMetricRegistry metrics) {
    }

    @Override
    public Status status() {
        return null;
    }

    @Override
    public void sendLoadMetrics(Map<Integer, Double> taskToLoad) {

    }

    @Override
    public void sendBackPressureStatus(BackPressureStatus bpStatus) {

    }

    @Override
    public void send(Iterator<TaskMessage> msgs) {

    }

    @Override
    public Map<Integer, Load> getLoad(Collection<Integer> tasks) {
        return Map.of();
    }

    @Override
    public int getPort() {
        return 0;
    }

    @Override
    public void close() {

    }
}
