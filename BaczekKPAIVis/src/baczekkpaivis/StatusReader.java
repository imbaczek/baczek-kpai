/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package baczekkpaivis;

import java.io.BufferedReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Logger;
import javax.vecmath.Point2d;

/**
 *
 * @author baczyslaw
 */
public class StatusReader {
    private BufferedReader reader;
    private List<Unit> friendlies = new ArrayList<Unit>();
    private List<Unit> enemies = new ArrayList<Unit>();
    private List<Point2d> geovents = new ArrayList<Point2d>();
    private int mapHeight;
    private int mapWidth;
    private int frameNum;

    StatusReader(BufferedReader r) {
        reader = r;
    }

    /* example file:
    frame 85
    map 256 256
    geovents
        1000 100 1000
        2000 100 2000
    units friendly
        kernel 1209 256 101 256
    units enemy
        kernel 1510 1792 100 1792
    influence map
        128 128
        I1,1 I1,2 ... I1,128
        ... */

    private static final int MODE_GEO = 1;
    private static final int MODE_FRIENDS = 2;
    private static final int MODE_ENEMIES = 3;
    private static final int MODE_INFLUENCE_START = 4;
    private static final int MODE_INFLUENCE_MAP = 5;

    public void parse() throws IOException {
        String line;
        int mode = 0;
        while ((line = reader.readLine()) != null) {
            if (line.startsWith("frame")) {
                parseFrame(line);
            } else if (line.startsWith("map")) {
                parseMap(line);
            } else if (line.startsWith("geovents")) {
                mode = MODE_GEO;
            } else if (line.startsWith("units friendly")) {
                mode = MODE_FRIENDS;
            } else if (line.startsWith("units enemy")) {
                mode = MODE_ENEMIES;
            } else if (line.startsWith("influence map")) {
                mode = MODE_INFLUENCE_START;
            } else {
                if (!line.startsWith("\t")) {
                    Logger.getLogger("StatusReader").info("invalid status line");
                    continue;
                }
                switch (mode) {
                    case MODE_GEO:
                        parseGeo(line);
                        break;
                    case MODE_FRIENDS:
                        parseFriend(line);
                        break;
                    case MODE_ENEMIES:
                        parseEnemy(line);
                        break;
                    case MODE_INFLUENCE_START:
                        break;
                    case MODE_INFLUENCE_MAP:
                        break;
                    default:
                }
            }
        }
    }

    private void parseEnemy(String line) {
        Unit u = parseUnit(line);
        getEnemies().add(u);
    }

    private void parseFrame(String line) {
        String[] split = line.split("\\s+");
        frameNum = Integer.parseInt(split[1]);
    }

    private void parseFriend(String line) {
        Unit u = parseUnit(line);
        getFriendlies().add(u);
    }

    private void parseGeo(String line) {
        String[] split = line.split("\\s+");
        float x = Float.parseFloat(split[1]);
        float z = Float.parseFloat(split[2]);
        Point2d pt = new Point2d(x, z);
        getGeovents().add(pt);
    }

    private void parseMap(String line) {
        String[] split = line.split("\\s+");
        mapHeight = Integer.parseInt(split[2]);
        mapWidth = Integer.parseInt(split[1]);
    }

    private Unit parseUnit(String line) throws NumberFormatException {
        String[] split = line.split("\\s+");
        String name = split[1];
        String id = split[2];
        float x = Float.parseFloat(split[3]);
        float z = Float.parseFloat(split[5]);
        Point2d pt = new Point2d(x, z);
        Unit u = new Unit(pt, name);
        return u;
    }

    /**
     * @return the friendlies
     */
    public List<Unit> getFriendlies() {
        return friendlies;
    }

    /**
     * @return the enemies
     */
    public List<Unit> getEnemies() {
        return enemies;
    }

    /**
     * @return the geovents
     */
    public List<Point2d> getGeovents() {
        return geovents;
    }

    /**
     * @return the mapHeight
     */
    public int getMapHeight() {
        return mapHeight;
    }

    /**
     * @return the mapWidth
     */
    public int getMapWidth() {
        return mapWidth;
    }

    /**
     * @return the frameNum
     */
    public int getFrameNum() {
        return frameNum;
    }
}
