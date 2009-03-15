/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
package baczekkpaivis;

import java.awt.Color;
import java.awt.Graphics;
import java.util.List;
import javax.swing.JPanel;
import javax.vecmath.Point2d;

/**
 *
 * @author baczyslaw
 */
public class DrawingPanel extends JPanel {

    private int mapw;
    private int maph;
    private int frameNum;
    private List<Point2d> geovents;
    private List<Unit> friendlies;
    private List<Unit> enemies;

    public static final int SQUARE_SIZE = 8;

    @Override
    public void paintComponent(Graphics g) {
        super.paintComponent(g);

        if (geovents == null || friendlies == null || enemies == null) {
            g.setColor(getForeground());
            g.drawString("data missing", 10, 10);
            return;
        }

        final int w = getWidth();
        final int h = getHeight();
        // scale ratios
        final float scalex = w / (float) getMapw() / SQUARE_SIZE;
        final float scaley = h / (float) getMaph() / SQUARE_SIZE;

        // draw geo spots
        g.setColor(Color.GREEN);
        for (Point2d pt : getGeovents()) {
            g.drawArc((int) (pt.x * scalex), (int) (pt.y * scaley), 4, 4, 0, 360);
        }

        // draw friendly units
        g.setColor(Color.BLUE);
        for (Unit u : getFriendlies()) {
            Point2d pt = u.getPos();
            g.drawRect((int) (pt.x * scalex), (int) (pt.y * scaley), 1, 1);
        }

        // draw enemy units
        g.setColor(Color.RED);
        for (Unit u : getEnemies()) {
            Point2d pt = u.getPos();
            g.drawRect((int) (pt.x * scalex), (int) (pt.y * scaley), 1, 1);
        }

        // draw friendly influence

        // draw enemy influence
        g.setColor(getForeground());
        g.drawString("frame "+frameNum, 4, h-4);
    }

    /**
     * @return the mapw
     */
    public int getMapw() {
        return mapw;
    }

    /**
     * @param mapw the mapw to set
     */
    public void setMapw(int mapw) {
        this.mapw = mapw;
    }

    /**
     * @return the maph
     */
    public int getMaph() {
        return maph;
    }

    /**
     * @param maph the maph to set
     */
    public void setMaph(int maph) {
        this.maph = maph;
    }

    /**
     * @return the frameNum
     */
    public int getFrameNum() {
        return frameNum;
    }

    /**
     * @param frameNum the frameNum to set
     */
    public void setFrameNum(int frameNum) {
        this.frameNum = frameNum;
        repaint();
    }

    /**
     * @return the geovents
     */
    public List<Point2d> getGeovents() {
        return geovents;
    }

    /**
     * @param geovents the geovents to set
     */
    public void setGeovents(List<Point2d> geovents) {
        this.geovents = geovents;
        repaint();
    }

    /**
     * @return the friendlies
     */
    public List<Unit> getFriendlies() {
        return friendlies;
    }

    /**
     * @param friendlies the friendlies to set
     */
    public void setFriendlies(List<Unit> friendlies) {
        this.friendlies = friendlies;
        repaint();
    }

    /**
     * @return the enemies
     */
    public List<Unit> getEnemies() {
        return enemies;
    }

    /**
     * @param enemies the enemies to set
     */
    public void setEnemies(List<Unit> enemies) {
        this.enemies = enemies;
        repaint();
    }
}
