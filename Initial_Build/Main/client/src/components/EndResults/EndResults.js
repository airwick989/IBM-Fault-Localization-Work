import { useEffect, useState } from "react";
import axios from 'axios';
import './styles.css';
import LocalizationResults from "../LocalizationResults/LocalizationResults";

function EndResults() {
    const [sourceFiles, setSourceFiles] = useState([]);
    const [synchRegions, setSynchRegions] = useState([]);
    const [aps, setAps] = useState({});

    const apDesc =`
    Hot Sections:
        A “Hot Section” anti-pattern is the most common lock
        contention performance example because it is the case where
        one specific object or area is causing a slowdown. Therefore,
        in general ”Hot Sections” come from one synchronized region
        or from one object monitor acting as a bottleneck for the entire
        program.

        Hot1:
            Due to a ”Hot Section” being a situation where one critical 
            section is the cause of a significant portion of the contention 
            issues in a system, there are multiple ways to create it. The 
            first type of ”Hot Section is where there is one critical section 
            that is the main cause of contention. This can be either 
            because the process inside the thread is very large and 
            that load causes the problem, or there is too much traffic 
            on the lock, which will cause a problem regardless of the 
            length of the process.
            
            Recommendation: This can be fixed by either reducing
            the time needed for the task inside the lock or by breaking
            apart the critical section where possible. If the problem is
            due to frequent access then fixing it would focus on how
            the flow of the system operates, the first solution would be
            to switch the critical section to a different style to allow
            it to have multiple users at once if possible, if that is
            not an option then you need to either significantly speed
            up the critical section or reduce the number of calls made 
            to it.

        Hot2:
            The second type of
            ”Hot Section” is where the one synchronized object is being
            accessed throughout the program. Similar top the previous
            section, this may be hard to spot due to the problem not
            occurring inside any single critical section, but having to do
            with the behavior of the program or with multiple sections
            combining to cause the problem.
            
            Recommendation: As this is not a problem due to one
            section, the solution works differently, the first option is to
            split the lock object to prevent them from interfering with
            each other this could be done by dividing the data that is
            to be accessed or by splitting some tasks into different lock
            objects. However, if this is not possible then the solutions
            are similar to Hot1, with switching the critical
            section to a different style or modifying the critical sections
            and reducing the overall number of calls.

        Hot3:
            The third type of contention is caused by loops in or around 
            critical sections, which at run time will appear like a Hot 
            Section-Type 1 code smell depending on the positioning and 
            functions of the loops. Determining if this would be a 
            problem with purely staticanalysis may be impossible due to 
            variables being set only during run time.

            Recommendation: Fixing the problem would change
            depending on where the loops were placed. In the first code
            example we have included the loop is inside the critical
            section which multiplies the execution time of the operation
            inside the critical section, in that case the solutions from Hot
            Section-Type 1 should apply.

    Overly Split:
        Overly split locks means that the locks have been significantly 
        divided, perhaps in an attempt to resolve a hot section, to the 
        extent that they begin to cause a problem for performance. The 
        delay in this is most likely due to the overhead of entering 
        and exiting a lock rather than the waiting or executing time 
        of the lock itself. In a previous paper we defined this as a 
        Type 2 contention.
        
        Recommendation: This could be resolved by lock merging
        or switching the synchronization method it that is not possible.
        Lock merging is combining the tasks in separate locks into
        larger single locks, reducing the granularity. If that is not
        an option, then switching to a data structure that supports
        simultaneous access could be an option that would allow you
        to remove the problem without changing the code significantly.

    Simultaneous:
        Simultaneous access is where the code is set up in a way that
        it allows a variable that should be synchronized to be accessed
        from elsewhere without permission. This can be done either
        by having the object accessed outside of a critical section by
        accident, or by accidentally having two objects interact with
        each other in an unexpected way.

        Recommendation: Due to this occurring unintentionally,
        locating and resolving it is difficult. The steps to resolve it
        are: confirm that this is the issue, locate it, and fix it. To
        confirm that this is a problem caused by simultaneous access,
        you would need to start by examining run time data in order
        to check the behavior of the locks, in this situation one of
        the locks should be under more pressure than expected, and
        another should not exist, this is because two or more have
        been merged accidentally. Once that is done and you have
        the problematic section you can check what is being used to
        synchronize it and that should show you what the problem
        is, then resolving it is simple.

    Unpredictable:
        Unpredictable outcome or bad form describes a situation
        where the performance is negatively affected by the code
        not properly handling synchronization in a way that causes
        locks and makes synchronized data interfere with other
        data unintentionally. This type of issue will usually result
        in strange unexplained behavior with both functional and
        performance issues.
    `


    useEffect(() => {

        const fetchResults = async () => {
            axios.get('http://localhost:5001/cds/getData', { params: { target: 'pattern_matcher.json', isMultiple: 'false' }})
            .then( (e) => {
                setSourceFiles(e.data['files']);
                setSynchRegions(e.data['synch_regions']);
                setAps(e.data['anti_patterns']);
            })
            .catch( (e) => {
                console.error('Error: ', e)
                alert('Error: ' + e)
            })
        };

        fetchResults();
    }, []);

    
    return ( 
        <div className="container" style={{height: '150vh', overflowY: 'scroll'}}>
            <LocalizationResults title="Final Results"/>

            <div className="stats bg-primary text-primary-content" style={{marginBottom: 50}}>
  
                <div className="stat" style={{display: 'flex', flexDirection: 'column', justifyContent:'flex-start'}}>
                    <div>
                        <div className="stat-title">Files Analyzed by Pattern Matcher</div>
                        <ul style={{listStyleType: 'disc'}}>
                            {sourceFiles.map( file => <li>{file}</li> )}
                        </ul>
                    </div>
                    <div style={{marginTop: 20}}>
                        <div className="stat-title">Anti-patterns Detected</div>
                        {Object.entries(aps).map(([key, value]) => (
                            <div key={key}>
                            <p><span style={{fontWeight: "bolder"}}>{key}</span>: {value}</p>
                            </div>
                        ))}
                        <div className="stat-actions">
                            <button onClick={ () => alert(apDesc)} className="btn btn-sm">Anti-pattern Information</button>
                        </div>
                    </div>
                </div>
                
                <div className="stat">
                    <div className="stat-title">Synchronized Regions Found</div>
               
                    {synchRegions.map( region => <pre>{region}<hr/></pre>)}
                    
                </div>
            
            </div>
        </div>
    );
}

export default EndResults;