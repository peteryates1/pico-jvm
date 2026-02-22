/*
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2024 Oren Sokoler (https://github.com/orenskl)
 *
 */

package pico.net.mqtt;

import java.io.IOException;
import pico.net.TCPSocket;

/**
 * A <code>MQTTClient</code> implements an MQTT 3.1.1 client over TCP.
 * This is a pure Java implementation that uses {@link TCPSocket} for
 * network communication. Supports CONNECT, PUBLISH, SUBSCRIBE, PINGREQ,
 * and DISCONNECT packet types.
 *
 * <p>QoS 0 (at most once) is used for all publish and subscribe operations.</p>
 */
public class MQTTClient {

    /* MQTT Control Packet types */
    private static final int CONNECT     = 1;
    private static final int CONNACK     = 2;
    private static final int PUBLISH     = 3;
    private static final int SUBSCRIBE   = 8;
    private static final int SUBACK      = 9;
    private static final int PINGREQ     = 12;
    private static final int PINGRESP    = 13;
    private static final int DISCONNECT  = 14;

    /* CONNACK return codes */
    private static final int CONNACK_ACCEPTED = 0;

    /**
     * The underlying TCP connection
     */
    private TCPSocket socket;

    /**
     * The next packet identifier for SUBSCRIBE
     */
    private int nextPacketId;

    /**
     * Buffer for building and receiving packets
     */
    private byte[] buffer;

    /**
     * Create a new MQTT client. Call {@link #connect} to establish
     * a connection to a broker.
     */
    public MQTTClient() {
        this.socket = null;
        this.nextPacketId = 1;
        this.buffer = new byte[512];
    }

    /**
     * Connect to an MQTT broker.
     *
     * @param broker   The broker hostname or IP address
     * @param port     The broker port (typically 1883)
     * @param clientId The MQTT client identifier
     *
     * @throws IOException if the connection or MQTT handshake fails
     */
    public void connect(String broker, int port, String clientId) throws IOException {
        this.socket = new TCPSocket(broker, port);

        /* Build CONNECT packet */
        int pos = 0;

        /* Variable header */
        byte[] varHeader = new byte[10];
        /* Protocol Name "MQTT" */
        varHeader[0] = 0;
        varHeader[1] = 4;
        varHeader[2] = (byte) 'M';
        varHeader[3] = (byte) 'Q';
        varHeader[4] = (byte) 'T';
        varHeader[5] = (byte) 'T';
        /* Protocol Level 4 (MQTT 3.1.1) */
        varHeader[6] = 4;
        /* Connect Flags: Clean Session */
        varHeader[7] = 0x02;
        /* Keep Alive: 60 seconds */
        varHeader[8] = 0;
        varHeader[9] = 60;

        /* Payload: Client ID */
        byte[] clientIdBytes = stringToBytes(clientId);
        int clientIdLen = clientIdBytes.length;

        int remainingLen = 10 + 2 + clientIdLen;

        /* Fixed header */
        pos = 0;
        buffer[pos++] = (byte) (CONNECT << 4);
        pos = encodeRemainingLength(buffer, pos, remainingLen);

        /* Variable header */
        arrayCopy(varHeader, 0, buffer, pos, 10);
        pos += 10;

        /* Client ID (length-prefixed UTF-8 string) */
        buffer[pos++] = (byte) ((clientIdLen >> 8) & 0xFF);
        buffer[pos++] = (byte) (clientIdLen & 0xFF);
        arrayCopy(clientIdBytes, 0, buffer, pos, clientIdLen);
        pos += clientIdLen;

        socket.send(buffer, 0, pos);

        /* Wait for CONNACK */
        int received = socket.receive(buffer, 0, buffer.length);
        if (received < 4) {
            throw new IOException();
        }
        int packetType = (buffer[0] >> 4) & 0x0F;
        if (packetType != CONNACK) {
            throw new IOException();
        }
        if (buffer[3] != CONNACK_ACCEPTED) {
            throw new IOException();
        }
    }

    /**
     * Publish a message to a topic (QoS 0).
     *
     * @param topic   The topic string
     * @param payload The message payload
     *
     * @throws IOException if the publish fails
     */
    public void publish(String topic, byte[] payload) throws IOException {
        if (socket == null) {
            throw new IOException();
        }

        byte[] topicBytes = stringToBytes(topic);
        int topicLen = topicBytes.length;
        int payloadLen = payload.length;

        int remainingLen = 2 + topicLen + payloadLen;

        int pos = 0;
        /* Fixed header: PUBLISH, QoS 0, no retain */
        buffer[pos++] = (byte) (PUBLISH << 4);
        pos = encodeRemainingLength(buffer, pos, remainingLen);

        /* Topic (length-prefixed UTF-8 string) */
        buffer[pos++] = (byte) ((topicLen >> 8) & 0xFF);
        buffer[pos++] = (byte) (topicLen & 0xFF);
        arrayCopy(topicBytes, 0, buffer, pos, topicLen);
        pos += topicLen;

        /* Payload (no packet ID for QoS 0) */
        arrayCopy(payload, 0, buffer, pos, payloadLen);
        pos += payloadLen;

        socket.send(buffer, 0, pos);
    }

    /**
     * Subscribe to a topic (QoS 0).
     *
     * @param topic The topic filter string
     *
     * @throws IOException if the subscribe fails
     */
    public void subscribe(String topic) throws IOException {
        if (socket == null) {
            throw new IOException();
        }

        byte[] topicBytes = stringToBytes(topic);
        int topicLen = topicBytes.length;

        int packetId = nextPacketId++;
        int remainingLen = 2 + 2 + topicLen + 1; /* packetId + topic + QoS */

        int pos = 0;
        /* Fixed header: SUBSCRIBE, must have bit 1 set */
        buffer[pos++] = (byte) ((SUBSCRIBE << 4) | 0x02);
        pos = encodeRemainingLength(buffer, pos, remainingLen);

        /* Packet Identifier */
        buffer[pos++] = (byte) ((packetId >> 8) & 0xFF);
        buffer[pos++] = (byte) (packetId & 0xFF);

        /* Topic Filter (length-prefixed UTF-8 string) */
        buffer[pos++] = (byte) ((topicLen >> 8) & 0xFF);
        buffer[pos++] = (byte) (topicLen & 0xFF);
        arrayCopy(topicBytes, 0, buffer, pos, topicLen);
        pos += topicLen;

        /* Requested QoS: 0 */
        buffer[pos++] = 0;

        socket.send(buffer, 0, pos);

        /* Wait for SUBACK */
        int received = socket.receive(buffer, 0, buffer.length);
        if (received < 5) {
            throw new IOException();
        }
        int packetType = (buffer[0] >> 4) & 0x0F;
        if (packetType != SUBACK) {
            throw new IOException();
        }
    }

    /**
     * Receive and parse the next MQTT message. This blocks until
     * a message is received.
     *
     * @param topicBuf  Buffer to store the topic string bytes
     * @param payloadBuf Buffer to store the payload bytes
     *
     * @return an array of two ints: [topicLength, payloadLength],
     *         or null if a non-PUBLISH packet was received
     * @throws IOException if the receive fails
     */
    public int[] receiveMessage(byte[] topicBuf, byte[] payloadBuf) throws IOException {
        if (socket == null) {
            throw new IOException();
        }

        int received = socket.receive(buffer, 0, buffer.length);
        if (received < 2) {
            throw new IOException();
        }

        int packetType = (buffer[0] >> 4) & 0x0F;
        if (packetType == PINGREQ) {
            /* Respond to PINGREQ with PINGRESP */
            buffer[0] = (byte) (PINGRESP << 4);
            buffer[1] = 0;
            socket.send(buffer, 0, 2);
            return null;
        }

        if (packetType != PUBLISH) {
            return null;
        }

        /* Decode remaining length */
        int pos = 1;
        int multiplier = 1;
        int remainingLen = 0;
        int encodedByte;
        do {
            encodedByte = buffer[pos++] & 0xFF;
            remainingLen += (encodedByte & 0x7F) * multiplier;
            multiplier *= 128;
        } while ((encodedByte & 0x80) != 0);

        /* Topic length */
        int topicLen = ((buffer[pos] & 0xFF) << 8) | (buffer[pos + 1] & 0xFF);
        pos += 2;

        /* Copy topic */
        int copyLen = topicLen;
        if (copyLen > topicBuf.length) {
            copyLen = topicBuf.length;
        }
        arrayCopy(buffer, pos, topicBuf, 0, copyLen);
        pos += topicLen;

        /* Payload */
        int payloadLen = remainingLen - 2 - topicLen;
        copyLen = payloadLen;
        if (copyLen > payloadBuf.length) {
            copyLen = payloadBuf.length;
        }
        arrayCopy(buffer, pos, payloadBuf, 0, copyLen);

        int[] result = new int[2];
        result[0] = topicLen;
        result[1] = payloadLen;
        return result;
    }

    /**
     * Send a PINGREQ to the broker to keep the connection alive.
     *
     * @throws IOException if the send fails
     */
    public void ping() throws IOException {
        if (socket == null) {
            throw new IOException();
        }
        buffer[0] = (byte) (PINGREQ << 4);
        buffer[1] = 0;
        socket.send(buffer, 0, 2);
    }

    /**
     * Disconnect from the MQTT broker and close the TCP connection.
     */
    public void disconnect() {
        if (socket != null) {
            try {
                buffer[0] = (byte) (DISCONNECT << 4);
                buffer[1] = 0;
                socket.send(buffer, 0, 2);
            } catch (IOException e) {
                /* Ignore errors on disconnect */
            }
            socket.close();
            socket = null;
        }
    }

    /*
     * Helper methods
     */

    /**
     * Encode the MQTT remaining length field.
     */
    private int encodeRemainingLength(byte[] buf, int pos, int length) {
        do {
            int encodedByte = length % 128;
            length = length / 128;
            if (length > 0) {
                encodedByte = encodedByte | 0x80;
            }
            buf[pos++] = (byte) encodedByte;
        } while (length > 0);
        return pos;
    }

    /**
     * Convert a String to a byte array (ASCII/Latin-1).
     */
    private byte[] stringToBytes(String s) {
        byte[] bytes = new byte[s.length()];
        for (int i = 0; i < s.length(); i++) {
            bytes[i] = (byte) s.charAt(i);
        }
        return bytes;
    }

    /**
     * Copy bytes between arrays (like System.arraycopy but safe for CLDC).
     */
    private static void arrayCopy(byte[] src, int srcOff, byte[] dst, int dstOff, int len) {
        for (int i = 0; i < len; i++) {
            dst[dstOff + i] = src[srcOff + i];
        }
    }

}
