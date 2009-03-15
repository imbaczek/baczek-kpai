/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package baczekkpaivis;

import javax.vecmath.Point2d;

/**
 *
 * @author baczyslaw
 */
public class Unit {
    private Point2d pos;
    private String name;

    Unit(Point2d p, String n) {
        name = n;
        pos = p;
    }

    /**
     * @return the pos
     */
    public Point2d getPos() {
        return pos;
    }

    /**
     * @param pos the pos to set
     */
    public void setPos(Point2d pos) {
        this.pos = pos;
    }

    public void setPos(float x, float y) {
        this.pos = new Point2d(x, y);
    }

    /**
     * @return the name
     */
    public String getName() {
        return name;
    }

    /**
     * @param name the name to set
     */
    public void setName(String name) {
        this.name = name;
    }
}
