package CommMngr;

import org.apache.log4j.Logger;
import java.io.IOException;
import java.io.FileInputStream;
import java.io.ByteArrayOutputStream;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.Iterator;
import java.util.Set;
import javax.net.ssl.*;
import java.security.*;
import java.security.cert.*;
import redis.clients.jedis.Jedis;
import com.mongodb.MongoClientSettings;
import com.mongodb.client.MongoClients;
import com.mongodb.client.MongoClient;
import com.mongodb.client.MongoDatabase;
import com.mongodb.MongoException;
import org.msgpack.core.MessagePack;
import org.msgpack.core.MessageUnpacker;
import org.msgpack.core.MessagePacker;
import org.gmssl.*;

public class MngrWorkerSvr extends Thread{
    private String RedisUrl;
    private String RedisPwd;
    private String MongoUrl;
    private String SSLKeyStore;
    private String SSLKeyStorePwd;
    private int Port;
    static private Logger Log;
    MongoDatabase Database;
    Jedis Redis;

    public MngrWorkerSvr (int Port, String MongoUrl, String RedisUrl, String RedisPwd,
                          String SSLKeyStore, String SSLKeyStorePwd) {
        Log = Logger.getLogger(MngrWorkerSvr.class);
        this.Port = Port;
        this.MongoUrl = MongoUrl;
        this.RedisUrl = RedisUrl;
        this.RedisPwd = RedisPwd;
        this.SSLKeyStore = SSLKeyStore;
        this.SSLKeyStorePwd = SSLKeyStorePwd;

        this.Log.info("DB URL: " + this.MongoUrl);
        this.Log.info("Redis URL: " + this.RedisUrl);
        this.Log.info("Redis Password: " + this.RedisPwd);
        this.Log.info("SSLKeyStore: " + this.SSLKeyStore);
        this.Log.info("Port: " + this.Port);
    }

    @Override
    public void run() {
        try {
            startServer();
        } catch (IOException e) {
            Log.error(e);
        }
    }

    public void startServer() throws IOException {
        // 创建 Jedis 对象并连接到 Redis
        Log.info("Server started, listening on port " + Port);
        String[] redisParts = RedisUrl.split(":");
        String host = redisParts[0];
        int redisPort = Integer.parseInt(redisParts[1]);
        Redis = new Jedis(host, redisPort);
        try {
            Redis.auth(RedisPwd);
            Redis.ping();
            Log.info("Successfully connected to Redis server at " + RedisUrl);
        } catch (Exception e) {
            Log.error("Failed to connect to Redis server at " + RedisUrl);
            Log.error(e);
            throw e;
        }
        // 创建 mongoClient 对象并连接到 mongod
        try {
            MongoClient mongoClient = MongoClients.create(MongoUrl);
            Database = mongoClient.getDatabase("Comm");
            Log.info("Connected to database: " + Database.getName());
        } catch (MongoException e) {
            Log.error("Failed to connect to mongo server at " + MongoUrl);
            Log.error(e);
            throw e;
        }
        // 加载密钥库
        SSLContext sslContext = initializeSSLContext();
        try (ServerSocketChannel serverChannel = ServerSocketChannel.open()) {
            serverChannel.configureBlocking(false);
            serverChannel.socket().bind(new InetSocketAddress(Port));
            Selector selector = Selector.open();
            serverChannel.register(selector, SelectionKey.OP_ACCEPT);

            while (true) {
                selector.select();  // Block until at least one channel is ready for the operations you are interested in.
                Set<SelectionKey> selectedKeys = selector.selectedKeys();
                Iterator<SelectionKey> iter = selectedKeys.iterator();
                while (iter.hasNext()) {
                    SelectionKey key = iter.next();
                    if (key.isAcceptable()) {
                        acceptConnection(serverChannel, sslContext, selector);
                    }
                    if (key.isReadable()) {
                        try {
                            handleRead(key);
                        } catch (IOException e) {
                            key.cancel();
                            key.channel().close();
                        }
                    }
                    iter.remove();
                }
            }
        }
    }

    private SSLContext initializeSSLContext() throws IOException {
        try {
            KeyStore keyStore = KeyStore.getInstance("PKCS12");
            keyStore.load(new FileInputStream(SSLKeyStore), SSLKeyStorePwd.toCharArray());
            KeyManagerFactory keyManagerFactory = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
            keyManagerFactory.init(keyStore, SSLKeyStorePwd.toCharArray());

            SSLContext sslContext = SSLContext.getInstance("TLS");
            sslContext.init(keyManagerFactory.getKeyManagers(), null, null);
            return sslContext;
        } catch (NoSuchAlgorithmException | KeyStoreException | CertificateException | UnrecoverableKeyException | KeyManagementException e) {
            Log.error("Failed to initialize SSLContext", e);
            throw new IOException("Failed to initialize SSLContext", e);
        }
    }

    private static void acceptConnection(ServerSocketChannel serverChannel, SSLContext sslContext, Selector selector) throws IOException {
        SocketChannel clientChannel = serverChannel.accept();
        if (clientChannel == null) 
            return; // 非阻塞模式下，accept()可能返回null

        clientChannel.configureBlocking(false);
        // Create SSL engine and attach it to the client channel
        SSLEngine sslEngine = sslContext.createSSLEngine();
        sslEngine.setUseClientMode(false);
        sslEngine.beginHandshake();

        // 创建一个新的会话状态对象来保存 SSLEngine 和其他会话信息
        SessionState state = new SessionState(sslEngine);
        if (sslEngine.getHandshakeStatus() == SSLEngineResult.HandshakeStatus.NEED_UNWRAP) {
            clientChannel.register(selector, SelectionKey.OP_READ, state);
        } else if (sslEngine.getHandshakeStatus() == SSLEngineResult.HandshakeStatus.NEED_WRAP) {
            clientChannel.register(selector, SelectionKey.OP_WRITE, state);
        }

        // do handshake
        if (doHandshake(clientChannel, state, selector)) {
            Log.info("accept new ssl connection");
        }
    }

    private static boolean doHandshake(SocketChannel channel, SessionState state, Selector selector) throws IOException {
        ByteBuffer myNetData = state.myNetData;
        ByteBuffer peerNetData = state.peerNetData;

        while (state.sslEngine.getHandshakeStatus() != SSLEngineResult.HandshakeStatus.FINISHED &&
            state.sslEngine.getHandshakeStatus() != SSLEngineResult.HandshakeStatus.NOT_HANDSHAKING) {
            switch (state.sslEngine.getHandshakeStatus()) {
                case NEED_UNWRAP:
                    int read = channel.read(peerNetData);
                    if (read == -1) {
                        channel.close();
                        return false;
                    }
                    peerNetData.flip();
                    SSLEngineResult result = state.sslEngine.unwrap(peerNetData, ByteBuffer.allocate(state.sslEngine.getSession().getApplicationBufferSize()));
                    peerNetData.compact();
                    runDelegatedTasks(state.sslEngine);
                    break;

                case NEED_WRAP:
                    myNetData.clear();
                    result = state.sslEngine.wrap(ByteBuffer.allocate(0), myNetData);
                    myNetData.flip();
                    while (myNetData.hasRemaining()) {
                        channel.write(myNetData);
                    }
                    break;

                case NEED_TASK:
                    runDelegatedTasks(state.sslEngine);
                    break;

                default:
                    return false;
            }
        }
        channel.register(selector, SelectionKey.OP_READ, state);
        return true;
    }

    private static void runDelegatedTasks(SSLEngine sslEngine) {
        Runnable task;
        while ((task = sslEngine.getDelegatedTask()) != null) {
            task.run();
        }
    }

    private static void handleRead(SelectionKey key) throws IOException {
        SocketChannel channel = (SocketChannel) key.channel();
        SessionState state = (SessionState) key.attachment();
        ByteBuffer buffer = ByteBuffer.allocate(1024);  // Adjust size as needed

        int read = channel.read(buffer);
        if (read == -1) {
            key.cancel();
            channel.close();
            Log.warn("error happen when channel read, close it");
            return;
        }
        buffer.flip();
        
        try {
            ByteBuffer decryptedData = ByteBuffer.allocate(state.sslEngine.getSession().getApplicationBufferSize());
            SSLEngineResult result = state.sslEngine.unwrap(buffer, decryptedData);
            if (result.getStatus() == SSLEngineResult.Status.OK) {
                decryptedData.flip();
                handleDecryptedData(decryptedData, channel, state.sslEngine);
            } else if (result.getStatus() == SSLEngineResult.Status.CLOSED) {
                channel.close();
                key.cancel();
                Log.info("connect close by peer, close it");
            } else {
                Log.info("Unknown status " + result.getStatus());
            }
        } catch (SSLException e) {
            Log.error("SSL error during reading", e);
            key.cancel();
            channel.close();
        }
    }

    private static void handleDecryptedData(ByteBuffer decryptedData, SocketChannel channel, SSLEngine sslEngine) throws IOException {
        byte[] data = new byte[decryptedData.remaining()];
        decryptedData.get(data);
        try (MessageUnpacker unpacker = MessagePack.newDefaultUnpacker(data)) {
            int mapSize = unpacker.unpackMapHeader();
            for (int i = 0; i < mapSize; i++) {
                int fieldKey = unpacker.unpackInt();
                if (fieldKey == 1) {
                    // msgBase
                    int msgBaseMapSize = unpacker.unpackMapHeader();
                    for (int j = 0; j < msgBaseMapSize; j++) {
                        int key = unpacker.unpackInt();
                        if (key == 1) {
                            Log.info("Recv Msg - msgType: " + unpacker.unpackInt());
                        } else {
                            unpacker.unpackValue();
                        }
                    }
                } else {
                    unpacker.unpackValue();
                }
            }
        } catch (Exception e) {
            Log.error("try parse message failed, length " + data.length, e);
            throw new IOException("Failed to parse message", e);
        }

        // create response: MsgPayload { msgBase { msgType: 2 } }
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        try (MessagePacker packer = MessagePack.newDefaultPacker(out)) {
            packer.packMapHeader(1);   // MsgPayload: 1 field
            packer.packInt(1);         // field 1 = msgBase
            packer.packMapHeader(1);   // MsgBase: 1 field
            packer.packInt(1);         // field 1 = msgType
            packer.packInt(2);         // value
        }
        byte[] responseBytes = out.toByteArray();
        
        ByteBuffer responseBuffer = ByteBuffer.wrap(responseBytes);
        ByteBuffer myNetData = ByteBuffer.allocate(sslEngine.getSession().getPacketBufferSize());
        
        SSLEngineResult result = sslEngine.wrap(responseBuffer, myNetData);
        myNetData.flip();

        if (result.getStatus() == SSLEngineResult.Status.OK) {
            while (myNetData.hasRemaining()) {
                channel.write(myNetData);
            }
        } else {
            Log.error("SSLEngine wrap result: " + result.getStatus());
        }
        Log.info("Send Msg: response msgType=2");
    }

    public static class SessionState {
        public final SSLEngine sslEngine;
        public final ByteBuffer myNetData;
        public final ByteBuffer peerNetData;

        public SessionState(SSLEngine sslEngine) {
            this.sslEngine = sslEngine;
            int netBufferSize = sslEngine.getSession().getPacketBufferSize();
            this.myNetData = ByteBuffer.allocate(netBufferSize);
            this.peerNetData = ByteBuffer.allocate(netBufferSize);
        }
    }
}