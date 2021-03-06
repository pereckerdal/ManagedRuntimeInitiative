/*
 * Copyright 1998-1999 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Sun designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Sun in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 */

package com.sun.tools.example.debug.bdi;

import com.sun.jdi.ThreadGroupReference;
import com.sun.jdi.ThreadReference;
import java.util.List;
import java.util.Stack;
import java.util.ArrayList;
import java.util.Iterator;

/**
 * Descend the tree of thread groups.
 * @author Robert G. Field
 */
public class ThreadGroupIterator implements Iterator {
    private final Stack<Iterator<ThreadGroupReference>> stack
                        = new Stack<Iterator<ThreadGroupReference>>();

    public ThreadGroupIterator(List<ThreadGroupReference> tgl) {
        push(tgl);
    }

    public ThreadGroupIterator(ThreadGroupReference tg) {
        List<ThreadGroupReference> tgl = new ArrayList<ThreadGroupReference>();
        tgl.add(tg);
        push(tgl);
    }

/*
    ThreadGroupIterator() {
        this(Env.vm().topLevelThreadGroups());
    }
*/

    private Iterator top() {
        return (Iterator)stack.peek();
    }

    /**
     * The invariant in this class is that the top iterator
     * on the stack has more elements.  If the stack is
     * empty, there is no top.  This method assures
     * this invariant.
     */
    private void push(List<ThreadGroupReference> tgl) {
        stack.push(tgl.iterator());
        while (!stack.isEmpty() && !top().hasNext()) {
            stack.pop();
        }
    }

    public boolean hasNext() {
        return !stack.isEmpty();
    }

    public Object next() {
        return nextThreadGroup();
    }

    public ThreadGroupReference nextThreadGroup() {
        ThreadGroupReference tg = (ThreadGroupReference)top().next();
        push(tg.threadGroups());
        return tg;
    }

    public void remove() {
        throw new UnsupportedOperationException();
    }

/*
    static ThreadGroupReference find(String name) {
        ThreadGroupIterator tgi = new ThreadGroupIterator();
        while (tgi.hasNext()) {
            ThreadGroupReference tg = tgi.nextThreadGroup();
            if (tg.name().equals(name)) {
                return tg;
            }
        }
        return null;
    }
*/
}
