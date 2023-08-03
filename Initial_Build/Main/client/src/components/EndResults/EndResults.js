import { useEffect, useState } from "react";
import axios from 'axios';
import './styles.css';
import LocalizationResults from "../LocalizationResults/LocalizationResults";

function EndResults() {
    const [sourceFiles, setSourceFiles] = useState([]);
    const [synchRegions, setSynchRegions] = useState([]);
    const [aps, setAps] = useState({});


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