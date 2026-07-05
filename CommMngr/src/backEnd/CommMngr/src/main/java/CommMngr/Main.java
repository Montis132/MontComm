package CommMngr;

import java.io.IOException;
import java.util.Properties;
import org.apache.log4j.Logger;
import java.io.OutputStream;
import java.io.PrintStream;

public class Main {
    private int WorkerSvrPort;
    private int HttpSvrPort;
    private String SSLKeyStore;
    private String SSLKeyStorePwd;
    private String RedisUrl;
    private String RedisPwd;
    private String MongoUrl;
    private Logger Log;

    private void readConf() throws IOException {
        Properties properties = new Properties();
        properties.load(Main.class.getResourceAsStream("/config.properties"));

        this.MongoUrl = properties.getProperty("mongodb.url");

        this.RedisUrl = properties.getProperty("redis.url");
        this.RedisPwd = properties.getProperty("redis.password");

        try {
            this.WorkerSvrPort = Integer.parseInt(properties.getProperty("WorkerSvrPort"));
        } catch (NumberFormatException e) {
            throw new IOException("WorkerSvrPort must be a valid integer value");
        }

        try {
            this.HttpSvrPort = Integer.parseInt(properties.getProperty("HttpSvrPort"));
        } catch (NumberFormatException e) {
            throw new IOException("HttpSvrPort must be a valid integer value");
        }

        try {
            this.SSLKeyStore = properties.getProperty("server.ssl.key-store");
        } catch (NumberFormatException e) {
            throw new IOException("server.ssl.key-store must be a valid integer value");
        }

        try {
            this.SSLKeyStorePwd = properties.getProperty("server.ssl.key-store-password");
        } catch (NumberFormatException e) {
            throw new IOException("server.ssl.key-store-password must be a valid integer value");
        }
    }

    public static void main(String[] args) {
        Main main = new Main();
        main.Log = Logger.getLogger(Main.class);
        main.Log.info("----------------------------------------------------------------------------");
        main.Log.info("----------------------------- CommMngr start -----------------------------");
        main.Log.info("----------------------------------------------------------------------------");
        // read conf
        try {
            main.readConf();
        } catch (IOException e) {
            main.Log.info(e);
            System.exit(1); // 退出程序
        }
        // start svr worker
        main.Log.info("Starting workerServer");
        MngrWorkerSvr workerServer = new MngrWorkerSvr(main.WorkerSvrPort, main.MongoUrl,
                                                        main.RedisUrl, main.RedisPwd,
                                                        main.SSLKeyStore, main.SSLKeyStorePwd);
        workerServer.start();
        // start mngr svr
        main.Log.info("Starting httpServer");
        MngrHttpSvr httpServer = new MngrHttpSvr(main.HttpSvrPort, main.MongoUrl,
                                                        main.RedisUrl, main.RedisPwd,
                                                        main.SSLKeyStore, main.SSLKeyStorePwd);
        httpServer.start();
        // close std out
        main.Log.info("close stdout");
        StringBuilder stringBuilder = new StringBuilder();
        OutputStream outputStream = new OutputStream() {
            @Override
            public void write(int b) throws IOException {
                // 将输出写入到 StringBuilder
                stringBuilder.append((char) b);
            }
        };
        PrintStream printStream = new PrintStream(outputStream);
        System.setOut(printStream);
        // close stderr
        main.Log.info("close stderr");
        OutputStream errorStream = new OutputStream() {
            @Override
            public void write(int b) throws IOException {
                // Write output to StringBuilder
                stringBuilder.append((char) b);
            }
        };
        PrintStream errorPrintStream = new PrintStream(errorStream);
        System.setErr(errorPrintStream);
    }
}
