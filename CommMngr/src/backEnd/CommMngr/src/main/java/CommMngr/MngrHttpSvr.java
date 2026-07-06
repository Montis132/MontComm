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
import org.bson.Document;
import com.mongodb.client.MongoCollection;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.UUID;
import java.nio.file.Files;
import java.io.File;
import java.io.InputStream;

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

    public static final String V1_ServerInfo_Key = "/api/v1/server/info";
    public static final String V1_Domain_Key = "/api/v1/domain";
    public static final String V1_KeyImport_Key = "/api/v1/key/import";

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
        server.createContext(Register_Key, new MngrHttpHandler(Log, Register_Key, Database, Redis));
        server.createContext(Login_Key, new MngrHttpHandler(Log, Login_Key, Database, Redis));
        server.createContext(GetAllClientMap_Key, new MngrHttpHandler(Log, GetAllClientMap_Key, Database, Redis));
        server.createContext(GetBestServer_Key, new MngrHttpHandler(Log, GetBestServer_Key, Database, Redis));
        server.createContext(GetAllIntraDomain_Key, new MngrHttpHandler(Log, GetAllIntraDomain_Key, Database, Redis));
        server.createContext(GetIntraDomainForClient_key, new MngrHttpHandler(Log, GetIntraDomainForClient_key, Database, Redis));
        server.createContext(PostIntraDomain_Key, new MngrHttpHandler(Log, PostIntraDomain_Key, Database, Redis));
        server.createContext(V1_ServerInfo_Key, new MngrHttpHandler(Log, V1_ServerInfo_Key, Database, Redis));
        server.createContext(V1_Domain_Key, new MngrHttpHandler(Log, V1_Domain_Key, Database, Redis));
        server.createContext(V1_KeyImport_Key, new MngrHttpHandler(Log, V1_KeyImport_Key, Database, Redis));
        // 设置线程池的大小
        server.setExecutor(null);
        server.start(); // 启动服务器
    }

    static class MngrHttpHandler implements HttpHandler {
        private String Key;
        private Logger Log;
        private MongoDatabase Database;
        private Jedis Redis;
        public MngrHttpHandler(Logger Log, String InputKey, MongoDatabase Database, Jedis Redis) {
            this.Log = Log;
            Key = InputKey;
            this.Database = Database;
            this.Redis = Redis;
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
        private static String getRole(HttpExchange exchange) {
            String role = exchange.getRequestHeaders().getFirst("X-Role");
            return role != null ? role : "user";
        }
        private static boolean checkPermission(String role, String... allowedRoles) {
            if ("manager".equals(role)) return true;
            for (String allowed : allowedRoles) {
                if (allowed.equals(role)) return true;
            }
            return false;
        }
        private static String readBody(HttpExchange exchange) throws IOException {
            try (InputStream is = exchange.getRequestBody()) {
                return new String(is.readAllBytes(), StandardCharsets.UTF_8);
            }
        }
        private static void sendJson(HttpExchange exchange, int status, String json) throws IOException {
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            byte[] bytes = json.getBytes(StandardCharsets.UTF_8);
            exchange.sendResponseHeaders(status, bytes.length);
            OutputStream os = exchange.getResponseBody();
            os.write(bytes);
            os.close();
        }
        private static void sendError(HttpExchange exchange, int status, String msg) throws IOException {
            String json = "{\"error\":\"" + msg + "\"}";
            sendJson(exchange, status, json);
        }
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String response = "NULL";
            boolean responseSent = false;

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
                    case V1_ServerInfo_Key:
                        if (!"GET".equals(exchange.getRequestMethod())) {
                            sendError(exchange, 405, "Method not allowed");
                            responseSent = true;
                            break;
                        }
                        if (!checkPermission(getRole(exchange), "user", "partner")) {
                            sendError(exchange, 403, "Forbidden");
                            responseSent = true;
                            break;
                        }
                        handleServerInfo(exchange);
                        responseSent = true;
                        break;
                    case V1_Domain_Key:
                        if (!checkPermission(getRole(exchange), "manager")) {
                            sendError(exchange, 403, "Forbidden");
                            responseSent = true;
                            break;
                        }
                        handleDomainRequest(exchange);
                        responseSent = true;
                        break;
                    case V1_KeyImport_Key:
                        if (!"POST".equals(exchange.getRequestMethod())) {
                            sendError(exchange, 405, "Method not allowed");
                            responseSent = true;
                            break;
                        }
                        if (!checkPermission(getRole(exchange), "partner", "user")) {
                            sendError(exchange, 403, "Forbidden");
                            responseSent = true;
                            break;
                        }
                        handleKeyImport(exchange);
                        responseSent = true;
                        break;
                    default:
                        response = "UnKnown Key" + "Key";
                        break;
                }
            } catch (IOException e) {
                Log.error(e);
                throw e;
            }

            if (!responseSent) {
                exchange.getResponseHeaders().set("Content-Type", "text/plain");
                exchange.sendResponseHeaders(200, response.length());
                OutputStream os = exchange.getResponseBody();
                os.write(response.getBytes());
                os.close();
            }
        }
        private void handleServerInfo(HttpExchange exchange) throws IOException {
            try {
                Sm2Key sm2Key = new Sm2Key();
                sm2Key.generateKey();
                byte[] publicKeyInfo = sm2Key.exportPublicKeyInfoDer();
                String publicKeyB64 = Base64.getEncoder().encodeToString(publicKeyInfo);
                String json = "{\"name\":\"CommServer\",\"address\":\"192.168.137.101:14001\",\"publicKey\":\"" + publicKeyB64 + "\"}";
                sendJson(exchange, 200, json);
            } catch (Exception e) {
                Log.error("Failed to get server info", e);
                sendError(exchange, 500, "Internal server error");
            }
        }
        private void handleDomainRequest(HttpExchange exchange) throws IOException {
            String method = exchange.getRequestMethod();
            String path = exchange.getRequestURI().getPath();
            if ("/api/v1/domain".equals(path)) {
                if ("GET".equals(method)) {
                    handleDomainList(exchange);
                } else if ("POST".equals(method)) {
                    handleDomainCreate(exchange);
                } else {
                    sendError(exchange, 405, "Method not allowed");
                }
            } else if (path.startsWith("/api/v1/domain/")) {
                String id = path.substring("/api/v1/domain/".length());
                if (id.isEmpty()) {
                    sendError(exchange, 400, "Bad request");
                    return;
                }
                if ("PUT".equals(method)) {
                    handleDomainUpdate(exchange, id);
                } else if ("DELETE".equals(method)) {
                    handleDomainDelete(exchange, id);
                } else {
                    sendError(exchange, 405, "Method not allowed");
                }
            } else {
                sendError(exchange, 400, "Bad request");
            }
        }
        private void handleDomainList(HttpExchange exchange) throws IOException {
            try {
                MongoCollection<Document> collection = Database.getCollection("Domains");
                StringBuilder json = new StringBuilder("[");
                boolean first = true;
                for (Document doc : collection.find()) {
                    if (!first) json.append(",");
                    json.append(doc.toJson());
                    first = false;
                }
                json.append("]");
                sendJson(exchange, 200, json.toString());
            } catch (Exception e) {
                Log.error("Failed to list domains", e);
                sendError(exchange, 500, "Internal server error");
            }
        }
        private void handleDomainCreate(HttpExchange exchange) throws IOException {
            try {
                String body = readBody(exchange);
                Document doc = Document.parse(body);
                doc.put("_id", UUID.randomUUID().toString());
                MongoCollection<Document> collection = Database.getCollection("Domains");
                collection.insertOne(doc);
                sendJson(exchange, 201, doc.toJson());
            } catch (Exception e) {
                Log.error("Failed to create domain", e);
                sendError(exchange, 500, "Internal server error");
            }
        }
        private void handleDomainUpdate(HttpExchange exchange, String id) throws IOException {
            try {
                String body = readBody(exchange);
                Document updateDoc = Document.parse(body);
                MongoCollection<Document> collection = Database.getCollection("Domains");
                Document filter = new Document("_id", id);
                Document set = new Document("$set", updateDoc);
                collection.updateOne(filter, set);
                sendJson(exchange, 200, "{\"status\":\"updated\"}");
            } catch (Exception e) {
                Log.error("Failed to update domain", e);
                sendError(exchange, 500, "Internal server error");
            }
        }
        private void handleDomainDelete(HttpExchange exchange, String id) throws IOException {
            try {
                MongoCollection<Document> collection = Database.getCollection("Domains");
                Document filter = new Document("_id", id);
                collection.deleteOne(filter);
                sendJson(exchange, 200, "{\"status\":\"deleted\"}");
            } catch (Exception e) {
                Log.error("Failed to delete domain", e);
                sendError(exchange, 500, "Internal server error");
            }
        }
        private void handleKeyImport(HttpExchange exchange) throws IOException {
            try {
                String pemBody = readBody(exchange);
                File tempFile = File.createTempFile("sm2pub_", ".pem");
                tempFile.deleteOnExit();
                Files.writeString(tempFile.toPath(), pemBody);
                Sm2Key sm2Key = new Sm2Key();
                sm2Key.importPublicKeyInfoPem(tempFile.getAbsolutePath());
                byte[] publicKeyDer = sm2Key.exportPublicKeyInfoDer();
                Document doc = new Document("_id", UUID.randomUUID().toString());
                doc.put("publicKeyDer", Base64.getEncoder().encodeToString(publicKeyDer));
                doc.put("createdAt", new java.util.Date().toString());
                MongoCollection<Document> collection = Database.getCollection("PublicKeys");
                collection.insertOne(doc);
                String json = "{\"status\":\"imported\",\"id\":\"" + doc.getString("_id") + "\"}";
                sendJson(exchange, 200, json);
            } catch (Exception e) {
                Log.error("Failed to import key", e);
                sendError(exchange, 500, "Failed to import key");
            }
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