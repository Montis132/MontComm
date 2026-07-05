package CommMngr;

import org.apache.log4j.Logger;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.Iterator;
import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;
import redis.clients.jedis.Jedis;
import com.mongodb.MongoClientSettings;
import com.mongodb.client.MongoClients;
import com.mongodb.client.MongoClient;
import com.mongodb.client.MongoDatabase;
import com.mongodb.MongoException;
import org.gmssl.*;

public class MngrHttpSvr extends Thread{
    private String RedisUrl;
    private String RedisPwd;
    private String MongoUrl;
    private String SSLKeyStore;
    private String SSLKeyStorePwd;
    private int Port;
    private Logger Log;
    MongoDatabase Database;
    Jedis Redis;
    static {
        System.loadLibrary("gmssljni");
    }
    
    public static final String Register_Key = "/api/register";
    public static final String Login_Key = "/api/login";
    public static final String GetAllClientMap_Key = "/api/getAllClientMap";
    public static final String GetAllIntraDomain_Key = "/api/getAllIntraDomain";
    public static final String GetBestServer_Key = "/api/getBestServer";
    public static final String GetIntraDomainForClient_key = "/api/getIntraDomainForClient";
    public static final String PostIntraDomain_Key = "/api/postIntraDomain";

    public MngrHttpSvr (int Port, String MongoUrl, String RedisUrl, String RedisPwd,
                        String SSLKeyStore, String SSLKeyStorePwd) {
        Log = Logger.getLogger(MngrHttpSvr.class);
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
            Log.info(e);
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
        // 创建httpserver
        HttpServer server = HttpServer.create(new InetSocketAddress(Port), 0);
        // handler map
        server.createContext(Register_Key, new MngrHttpHandler(Log, Register_Key));
        server.createContext(Login_Key, new MngrHttpHandler(Log, Login_Key));
        server.createContext(GetAllClientMap_Key, new MngrHttpHandler(Log, GetAllClientMap_Key));
        server.createContext(GetBestServer_Key, new MngrHttpHandler(Log, GetBestServer_Key));
        server.createContext(GetAllIntraDomain_Key, new MngrHttpHandler(Log, GetAllIntraDomain_Key));
        server.createContext(GetIntraDomainForClient_key, new MngrHttpHandler(Log, GetIntraDomainForClient_key));
        server.createContext(PostIntraDomain_Key, new MngrHttpHandler(Log, PostIntraDomain_Key));
        // 设置线程池的大小
        server.setExecutor(null);
        server.start(); // 启动服务器
    }

    static class MngrHttpHandler implements HttpHandler {
        private String Key;
        private Logger Log;
        public MngrHttpHandler(Logger Log, String InputKey) {
            this.Log = Log;
            Key = InputKey;
        }
        private static Map<String, String> parseQuery(String query) {
            Map<String, String> parameters = new HashMap<>();
            if (query != null) {
                String[] pairs = query.split("&");
                for (String pair : pairs) {
                    String[] keyValue = pair.split("=");
                    if (keyValue.length > 1) {
                        parameters.put(keyValue[0], keyValue[1]);
                    } else {
                        parameters.put(keyValue[0], "");
                    }
                }
            }
            return parameters;
        }
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            // 定义响应消息体
            String response = "NULL";

            try {
                switch (Key) {
                    case Register_Key:
                        handleRegister(exchange);
                        Log.error("Err");
                        response = "1111";
                        break;
                    case Login_Key:
                        response = "1111";
                        break;
                    case GetAllClientMap_Key :
                        if (!"GET".equals(exchange.getRequestMethod())) {
                            Log.warn("method " + Key + " cannot handle " + exchange.getRequestMethod());
                        }
                        response = GetAllClientMap_Key;
                        break;
                    case GetBestServer_Key :
                        if (!"GET".equals(exchange.getRequestMethod())) {
                            Log.warn("method " + Key + " cannot handle " + exchange.getRequestMethod());
                        }
                        response = GetBestServer_Key;
                        break;
                    case GetAllIntraDomain_Key :
                        if (!"GET".equals(exchange.getRequestMethod())) {
                            Log.warn("method " + Key + " cannot handle " + exchange.getRequestMethod());
                        }
                        String query = exchange.getRequestURI().getQuery();
                        Map<String, String> parameters = parseQuery(query);
                        for (String key : parameters.keySet()) {
                            Log.info("get params " + key + " " + parameters.get(key));
                        }
                        response = GetAllIntraDomain_Key;
                        break;
                    case GetIntraDomainForClient_key :
                        if (!"GET".equals(exchange.getRequestMethod())) {
                            Log.warn("method " + Key + " cannot handle " + exchange.getRequestMethod());
                        }
                        String qxQuery = exchange.getRequestURI().getQuery();
                        Map<String, String> qxParameters = parseQuery(qxQuery);
                        for (String key : qxParameters.keySet()) {
                            Log.info("get params " + key + " " + qxParameters.get(key));
                        }
                        if (!qxParameters.containsKey("ClientId")) {
                            break;
                        }
                        response = GetIntraDomainForClient_key;
                        break;
                    case PostIntraDomain_Key :
                        if (!"POST".equals(exchange.getRequestMethod())) {
                            Log.warn("method " + Key + " cannot handle " + exchange.getRequestMethod());
                        }
                        response = PostIntraDomain_Key;
                        break;
                    default:
                        response = "UnKnown Key" + "Key";
                        break;
                }
            } catch (IOException e) {
                Log.error(e);
                throw e;
            }

            // 设置响应头信息
            exchange.getResponseHeaders().set("Content-Type", "text/plain");
            exchange.sendResponseHeaders(200, response.length());
            // 获取输出流，发送响应消息
            OutputStream os = exchange.getResponseBody();
            os.write(response.getBytes());
            os.close();
        }
        public void handleRegister(HttpExchange exchange) throws IOException {
            try {
                Log.info("222222222");
                Sm2Key sm2_key = new Sm2Key();
                Log.info("111111111");
                sm2_key.generateKey();
                Log.info("111111111");
                byte[] privateKeyInfo = sm2_key.exportPrivateKeyInfoDer();
                byte[] publicKeyInfo = sm2_key.exportPublicKeyInfoDer();

                Sm2Key priKey = new Sm2Key();
                priKey.importPrivateKeyInfoDer(privateKeyInfo);
                priKey.exportEncryptedPrivateKeyInfoPem("Comm", "Sm2Pri.pem");

                Sm2Key pubKey = new Sm2Key();
                pubKey.importPublicKeyInfoDer(publicKeyInfo);
                pubKey.exportPublicKeyInfoPem("Sm2Pub.pem");
                Log.info("222222222");
            } catch (Exception e) {
                Log.error(e);
                throw new IOException("Failed to handle register", e);
            }
        }
    }
}