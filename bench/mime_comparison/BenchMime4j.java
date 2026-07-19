// Head-to-head MIME parsing speed benchmark: Apache James Mime4j driver.
// Same protocol as bench_libglot.cpp / bench_python.py / rustbench:
// read file, parse, walk every header and every part, decode text bodies.
import org.apache.james.mime4j.dom.Entity;
import org.apache.james.mime4j.dom.Message;
import org.apache.james.mime4j.dom.Multipart;
import org.apache.james.mime4j.dom.TextBody;
import org.apache.james.mime4j.message.DefaultMessageBuilder;
import org.apache.james.mime4j.stream.Field;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.Reader;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

public class BenchMime4j {
    static long sink = 0;

    static void walk(Entity e) throws IOException {
        if (e.getHeader() != null) {
            for (Field f : e.getHeader().getFields()) {
                sink += f.getName().length();
                String body = f.getBody();
                if (body != null) {
                    sink += body.length();
                }
            }
        }
        Object body = e.getBody();
        if (body instanceof Multipart) {
            for (Entity part : ((Multipart) body).getBodyParts()) {
                walk(part);
            }
        } else if (body instanceof Message) {
            walk((Message) body);
        } else if (body instanceof TextBody) {
            try (Reader r = ((TextBody) body).getReader()) {
                char[] buf = new char[8192];
                int n;
                while ((n = r.read(buf)) != -1) {
                    sink += n;
                }
            }
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1) {
            System.err.println("usage: BenchMime4j <filelist>");
            System.exit(2);
        }
        List<String> paths = new ArrayList<>();
        try (BufferedReader r = new BufferedReader(new FileReader(args[0]))) {
            String line;
            while ((line = r.readLine()) != null) {
                if (!line.isEmpty()) {
                    paths.add(line);
                }
            }
        }

        DefaultMessageBuilder builder = new DefaultMessageBuilder();
        builder.setMimeEntityConfig(org.apache.james.mime4j.stream.MimeConfig.PERMISSIVE);
        long parsed = 0, failed = 0;

        long start = System.nanoTime();
        for (String path : paths) {
            try (FileInputStream in = new FileInputStream(path)) {
                Message msg = builder.parseMessage(in);
                parsed++;
                walk(msg);
            } catch (Exception ex) {
                failed++;
            }
        }
        long elapsed = System.nanoTime() - start;
        double secs = elapsed / 1e9;

        System.out.printf(
            "java mime4j: %d files, %d parsed, %d failed, %.3fs, %.0f msg/s (sink=%d)%n",
            paths.size(), parsed, failed, secs, paths.size() / secs, sink);
    }
}
