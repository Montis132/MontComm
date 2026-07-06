package CommMngr;

public class UtilJNI {

    public static native byte[] randBytes(int len);

    public static native byte[] sm3Hash(byte[] input);

    public static native byte[] sm3Hmac(byte[] key, byte[] input);

    public static native byte[] sm4CbcEncrypt(byte[] plain, byte[] key);

    public static native byte[] sm4CbcDecrypt(byte[] cipher, byte[] key);

    public static native byte[] sm4ECBEncrypt(byte[] plain, byte[] key);

    public static native byte[] sm4ECBDecrypt(byte[] cipher, byte[] key);

    public static native String getVersion();

    static {
        System.loadLibrary("utiljni");
    }
}
