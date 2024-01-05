import Head from "next/head";
import styles from "@/styles/Home.module.css";
import React, { ReactElement, ReactNode, useEffect, useState } from "react";
import { Button, FormGroup, Switch, TextField } from "@mui/material";

const isDev = process.env.NODE_ENV === "development";

type ProofuinoActiveState = "ON" | "OFF";

interface Status {
  state: string;
  config: {
    targetTemperature: number;
  };
  sensors: {
    relay: "ON" | "OFF";
    temperatures: {
      dough: number;
      box: number;
    };
  };
}
export default function Home() {
  const [targetTemp, setTargetTemp] = useState<number>(0);
  const [status, setStatus] = useState<Status | null>(null);

  const fetchStatus = () => {
    if (isDev) {
      setStatus({
        state: "MOCKED",
        config: {
          targetTemperature: 999,
        },
        sensors: {
          relay: "ON",
          temperatures: {
            dough: 123,
            box: 123,
          },
        },
      });
      return;
    }
    fetch(`http://${window.location.host}/status`)
      .then((response) => response.json())
      .then((data) => {
        setStatus(data);
      })
      .catch((error) => console.error(error));
  };

  useEffect(() => {
    fetchStatus();
    if (isDev) return;
    const interval = setInterval(fetchStatus, 10000);
    return () => clearInterval(interval);
  }, []);

  const applyTargetTemperature = (newTargetTemp: number) => {
    fetch(`http://${window.location.host}/config`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        targetTemperature: newTargetTemp,
      }),
    }),
      //prevent full page reload which is the default behavior
      event?.preventDefault();
  };

  const applyProofuinoActiveState = (state: ProofuinoActiveState) => {
    if (isDev && status != null) {
      setStatus({
        ...status,
        state,
      });
      return;
    }
    fetch(`http://${window.location.host}/${state === "ON" ? "on" : "off"}`, {
      method: "POST",
    });
  };

  const withStatus = (f: (status: Status) => ReactNode) => {
    return status == null ? "Loading..." : f(status);
  };

  const Box: React.FunctionComponent<{
    children: ReactNode;
    title: string;
  }> = ({ children, title }) => {
    return (
      <div className={styles.infoBox}>
        <div className={styles.infoTitle}>{title}</div>
        <div className={styles.infoContent}>{children}</div>
      </div>
    );
  };

  const TextWithLabel: React.FunctionComponent<{
    label: string;
    text: string;
  }> = ({ label, text }) => (
    <div
      style={{
        padding: 3,
        display: "grid",
        gridTemplateColumns: "120px 1fr",
      }}
    >
      <div>{label}:</div>
      <b>{text}</b>
    </div>
  );

  return (
    <>
      <Head>
        <title>Proofuino</title>
      </Head>
      <main className={styles.spaContainer}>
        <div className={styles.header}>
          Proofuino
          {status != null && (
            <Switch
              defaultChecked
              checked={status.state != "PAUSED"}
              onChange={(e) => {
                let status: ProofuinoActiveState = e.target.checked
                  ? "ON"
                  : "OFF";
                applyProofuinoActiveState(status);
              }}
            />
          )}
        </div>
        <div className={styles.infoGrid}>
          <Box title="Sensors">
            {withStatus((status) => (
              <>
                <TextWithLabel
                  label="Dough"
                  text={`${status.sensors.temperatures.dough}°C`}
                />
                <TextWithLabel
                  label="Box"
                  text={`${status.sensors.temperatures.box}°C`}
                />
                <TextWithLabel label="Relay" text={status.sensors.relay} />
              </>
            ))}
          </Box>
          {status != null && status.state != "PAUSED" && (
            <>
              <Box title="Status">
                {withStatus((status) => (
                  <>
                    <TextWithLabel label="State" text={status.state} />
                    <TextWithLabel
                      label="Target Dough Temperature"
                      text={`${status.config.targetTemperature}°C`}
                    />
                  </>
                ))}
              </Box>

              <Box title="Controls">
                <form
                  style={{ marginTop: 10 }}
                  onSubmit={(e) => {
                    applyTargetTemperature(targetTemp);
                    e.preventDefault();
                  }}
                >
                  <FormGroup>
                    <TextField
                      id="outlined-basic"
                      label="Target Dough Temperature"
                      type="number"
                      variant="outlined"
                      onChange={(e) => {
                        let n = Number(e.target.value);
                        if (Number.isSafeInteger(n)) {
                          setTargetTemp(Number(e.target.value));
                        }
                      }}
                    />
                    <Button
                      variant="contained"
                      type="submit"
                      value={"Set"}
                      style={{ marginTop: 5 }}
                    >
                      Set
                    </Button>
                  </FormGroup>
                </form>
              </Box>
            </>
          )}
        </div>
      </main>
    </>
  );
}
