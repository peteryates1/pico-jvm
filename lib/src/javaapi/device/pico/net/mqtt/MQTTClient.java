/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.net.mqtt;

import java.io.IOException;

/**
 * A <code>MQTTClient</code> implements an MQTT 3.1.1 client backed by
 * the lwIP MQTT library. Supports QoS 0 and 1 for publish and subscribe.
 *
 * <p>Incoming messages are buffered in a native ring buffer and retrieved
 * via {@link #receive}.</p>
 */
public class MQTTClient {

    /** QoS 0: At most once delivery */
    public static final int QOS_0 = 0;

    /** QoS 1: At least once delivery */
    public static final int QOS_1 = 1;

    /** Connection status: accepted */
    public static final int CONNECT_ACCEPTED = 0;

    /** Connection status: disconnected */
    public static final int CONNECT_DISCONNECTED = 256;

    /**
     * Native client handle (index into C-side table)
     */
    private int handle = -1;

    /**
     * Create a new MQTT client instance.
     */
    public MQTTClient() {
    }

    /**
     * Connect to an MQTT broker.
     *
     * @param broker    the broker hostname or IP address
     * @param port      the broker port (typically 1883)
     * @param clientId  the MQTT client identifier
     * @throws IOException if the connection or MQTT handshake fails
     */
    public void connect(String broker, int port, String clientId) throws IOException {
        connect(broker, port, clientId, null, null);
    }

    /**
     * Connect to an MQTT broker with optional credentials.
     *
     * @param broker    the broker hostname or IP address
     * @param port      the broker port (typically 1883)
     * @param clientId  the MQTT client identifier
     * @param user      the username (null if not used)
     * @param pass      the password (null if not used)
     * @throws IOException if the connection or MQTT handshake fails
     */
    public void connect(String broker, int port, String clientId,
                        String user, String pass) throws IOException {
        handle = mqtt_connect(broker, port, clientId, user, pass);
        if (handle < 0) {
            throw new IOException("MQTT connect failed");
        }
    }

    /**
     * Publish a message to a topic.
     *
     * @param topic   the topic string
     * @param payload the message payload
     * @param qos     the QoS level (QOS_0 or QOS_1)
     * @param retain  true to set the retain flag
     * @throws IOException if the publish fails
     */
    public void publish(String topic, byte[] payload, int qos, boolean retain)
            throws IOException {
        if (handle < 0) {
            throw new IOException("Not connected");
        }
        int result = mqtt_publish(handle, topic, payload, payload.length,
                                  qos, retain ? 1 : 0);
        if (result < 0) {
            throw new IOException("MQTT publish failed");
        }
    }

    /**
     * Publish a message to a topic with QoS 0 and no retain.
     *
     * @param topic   the topic string
     * @param payload the message payload
     * @throws IOException if the publish fails
     */
    public void publish(String topic, byte[] payload) throws IOException {
        publish(topic, payload, QOS_0, false);
    }

    /**
     * Subscribe to a topic.
     *
     * @param topic the topic filter string
     * @param qos   the maximum QoS level (QOS_0 or QOS_1)
     * @throws IOException if the subscribe fails
     */
    public void subscribe(String topic, int qos) throws IOException {
        if (handle < 0) {
            throw new IOException("Not connected");
        }
        int result = mqtt_subscribe(handle, topic, qos);
        if (result < 0) {
            throw new IOException("MQTT subscribe failed");
        }
    }

    /**
     * Subscribe to a topic with QoS 0.
     *
     * @param topic the topic filter string
     * @throws IOException if the subscribe fails
     */
    public void subscribe(String topic) throws IOException {
        subscribe(topic, QOS_0);
    }

    /**
     * Unsubscribe from a topic.
     *
     * @param topic the topic filter string
     * @throws IOException if the unsubscribe fails
     */
    public void unsubscribe(String topic) throws IOException {
        if (handle < 0) {
            throw new IOException("Not connected");
        }
        int result = mqtt_unsubscribe(handle, topic);
        if (result < 0) {
            throw new IOException("MQTT unsubscribe failed");
        }
    }

    /**
     * Receive the next incoming MQTT message. This method blocks until
     * a message is available or the timeout expires.
     *
     * <p>The topic is written into <code>topicBuf</code> and the payload
     * into <code>payloadBuf</code>. The returned array contains
     * <code>[topicLength, payloadLength]</code>, or <code>null</code>
     * if the timeout expired with no message.</p>
     *
     * @param topicBuf   buffer to store the topic bytes
     * @param payloadBuf buffer to store the payload bytes
     * @param timeoutMs  maximum time to wait in milliseconds (0 = non-blocking)
     * @return an array of two ints [topicLength, payloadLength], or null on timeout
     * @throws IOException if an error occurs or the connection was lost
     */
    public int[] receive(byte[] topicBuf, byte[] payloadBuf, int timeoutMs)
            throws IOException {
        if (handle < 0) {
            throw new IOException("Not connected");
        }
        int result = mqtt_receive(handle, topicBuf, payloadBuf, timeoutMs);
        if (result < 0) {
            throw new IOException("MQTT receive failed");
        }
        if (result == 0) {
            return null; /* timeout, no message */
        }
        /* result encodes topicLen in high 16 bits, payloadLen in low 16 bits */
        int[] lengths = new int[2];
        lengths[0] = (result >> 16) & 0xFFFF;
        lengths[1] = result & 0xFFFF;
        return lengths;
    }

    /**
     * Check if the client is currently connected to a broker.
     *
     * @return true if connected
     */
    public boolean isConnected() {
        if (handle < 0) return false;
        return mqtt_is_connected(handle) != 0;
    }

    /**
     * Disconnect from the MQTT broker and release resources.
     */
    public void disconnect() {
        if (handle >= 0) {
            mqtt_disconnect(handle);
            handle = -1;
        }
    }

    /*
     * Native methods backed by lwIP mqtt_client
     */
    private static native int  mqtt_connect(String broker, int port,
                                            String clientId,
                                            String user, String pass);
    private static native int  mqtt_publish(int handle, String topic,
                                            byte[] payload, int len,
                                            int qos, int retain);
    private static native int  mqtt_subscribe(int handle, String topic, int qos);
    private static native int  mqtt_unsubscribe(int handle, String topic);
    private static native int  mqtt_receive(int handle, byte[] topicBuf,
                                            byte[] payloadBuf, int timeoutMs);
    private static native int  mqtt_is_connected(int handle);
    private static native void mqtt_disconnect(int handle);
}
